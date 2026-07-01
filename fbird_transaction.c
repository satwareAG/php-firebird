/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_FIREBIRD

#include "php_ini.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_transaction.h"
#include "php_fbird_connection.h"
#include "firebird_utils.h"
#include "fbird_classes.h"

/* -----------------------------------------------------------------------
 * _php_fbird_trans_res_from_zval() - extract le_trans resource from a zval
 *   that may be either a resource or a Firebird\Transaction IS_OBJECT.
 *   Returns NULL if type is wrong or resource is not le_trans.
 * --------------------------------------------------------------------- */
static zend_resource *_php_fbird_trans_res_from_zval(zval *zv)
{
	if (!zv) return NULL;
	ZVAL_DEREF(zv);
	if (Z_TYPE_P(zv) == IS_RESOURCE) {
		zend_resource *res = Z_RES_P(zv);
		return (res->type == le_trans) ? res : NULL;
	}
	if (Z_TYPE_P(zv) == IS_OBJECT &&
			instanceof_function(Z_OBJCE_P(zv), fbird_transaction_ce)) {
		return fbird_transaction_get_resource(Z_OBJ_P(zv));
	}
	return NULL;
}

#define ROLLBACK    0
#define COMMIT      1
#define RETAIN      2

void _php_fbird_free_trans(zend_resource *rsrc)
{
	fbird_transaction *trans = (fbird_transaction *)rsrc->ptr;
	unsigned short i;

	FBDEBUG("Cleaning up transaction resource...");

#ifndef PHP_WIN32
	/* Fork-safety check (Issue #22): Skip cleanup if we're in a forked child */
	if (IBG(init_pid) != 0 && getpid() != IBG(init_pid)) {
		FBDEBUG("Skipping transaction cleanup in forked child process");
		efree(trans);
		return;
	}
#endif

	/* OO API Only: All transactions use fbt_rollback() */
	if (trans->fbt_transaction != NULL) {
		FBDEBUG("Rolling back unhandled OO API transaction...");
		int res = fbt_rollback(trans->fbt_transaction, IB_STATUS);
		fbt_free(trans->fbt_transaction);
		trans->fbt_transaction = NULL;
		/* Fix #78: _php_fbird_error() calls php_error_docref()/zend_throw_exception()
		 * which access EG() globals that may already be destroyed during MSHUTDOWN.
		 * Guard with in_mshutdown to prevent SIGABRT. */
		if (res && !IBG(in_mshutdown)) {
			_php_fbird_error();
		}
	}

	/* Fix #79: During MSHUTDOWN, db_link[] pointers may be dangling — the
	 * connection resource (le_plink) destructor can run before the transaction
	 * resource destructor, freeing the fbird_db_link struct that db_link[i]
	 * points to. Traversing tr_list through a freed pointer is a Use-After-Free.
	 * Skip the tr_list cleanup entirely during shutdown; the connection struct
	 * is already gone (or about to be freed), so the list nodes will be
	 * reclaimed with the request pool anyway. */
	if (!IBG(in_mshutdown)) {
		for (i = 0; i < trans->link_cnt; ++i) {
			if (trans->db_link[i] != NULL) {
				fbird_tr_list **l;
				for (l = &trans->db_link[i]->tr_list; *l != NULL; l = &(*l)->next) {
					if ( (*l)->trans == trans) {
						fbird_tr_list *p = *l;
						*l = p->next;
						efree(p);
						break;
					}
				}
			}
		}
	}
	efree(trans);
}


#define TPB_MAX_SIZE 2048

void _php_fbird_populate_trans(zend_long trans_argl, zend_long trans_timeout, char *last_tpb, unsigned short *len)
{
	/* No explicit flags: leave TPB empty so Firebird uses its defaults. */
	if (trans_argl == PHP_FBIRD_DEFAULT) {
		*len = 0;
		return;
	}

	/*
	 * Use IXpbBuilder-based TPB construction via OO API.
	 * This replaces manual byte array construction with the modern
	 * Firebird 3.0+ builder pattern for cleaner, type-safe TPB generation.
	 *
	 * See: fbxpb_build_tpb() in firebird_utils.cpp
	 */
	unsigned int buffer_length = 0;
	ISC_STATUS local_status[ISC_STATUS_LENGTH] = {0};

	unsigned char *tpb_buffer = fbxpb_build_tpb(
		IBG(master_instance),
		trans_argl,
		trans_timeout,
		&buffer_length,
		local_status
	);

	if (tpb_buffer == NULL) {
		/* Fallback: if OO API fails, report error and return empty TPB */
		php_error_docref(NULL, E_WARNING, "DEBUG: fbxpb_build_tpb returned NULL, local_status[1]=%ld", (long)local_status[1]);
		*len = 0;
		return;
	}

	/* Copy to caller's buffer (limited by TPB_MAX_SIZE) */
	if (buffer_length > TPB_MAX_SIZE) {
		buffer_length = TPB_MAX_SIZE;
	}
	memcpy(last_tpb, tpb_buffer, buffer_length);
	*len = (unsigned short) buffer_length;

	/* Free the OO API allocated buffer */
	fbxpb_free_tpb(tpb_buffer);
}

void _php_fbird_populate_trans_from_array(zval *options, zend_long *trans_timeout, char *last_tpb, unsigned short *len)
{
	unsigned char *p = (unsigned char *) last_tpb;
	unsigned char *end = p + TPB_MAX_SIZE;
	zval *tmp;

	/* TPB version */
	*p++ = isc_tpb_version3;

	/* access mode */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "access_mode", sizeof("access_mode") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			if (Z_LVAL_P(tmp) & PHP_FBIRD_READ) {
				*p++ = isc_tpb_read;
			} else if (Z_LVAL_P(tmp) & PHP_FBIRD_WRITE) {
				*p++ = isc_tpb_write;
			}
		}
	}

	/* isolation level */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "isolation", sizeof("isolation") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			zend_long iso = Z_LVAL_P(tmp);
			if (iso & PHP_FBIRD_COMMITTED) {
				*p++ = isc_tpb_read_committed;
				if (iso & PHP_FBIRD_REC_VERSION) {
					*p++ = isc_tpb_rec_version;
				} else if (iso & PHP_FBIRD_REC_NO_VERSION) {
					*p++ = isc_tpb_no_rec_version;
				}
			} else if (iso & PHP_FBIRD_CONSISTENCY) {
				*p++ = isc_tpb_consistency;
			} else if (iso & PHP_FBIRD_CONCURRENCY) {
				*p++ = isc_tpb_concurrency;
			}
		}
	}

	/* lock resolution */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "lock_resolution", sizeof("lock_resolution") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			zend_long res = Z_LVAL_P(tmp);
			if (res & PHP_FBIRD_NOWAIT) {
				*p++ = isc_tpb_nowait;
			} else if (res & PHP_FBIRD_WAIT) {
				*p++ = isc_tpb_wait;
			}
		}
	} else if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "wait", sizeof("wait") - 1)) != NULL) {
		/* BC for simpler key 'wait' => true/false? No, prefer explicit constants. */
		/* Let's stick to RFC constants. */
	}

	/* lock_timeout */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "lock_timeout", sizeof("lock_timeout") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			zend_long timeout = Z_LVAL_P(tmp);
			if (timeout > 0 && timeout <= 0x7FFF) {
				/* If we have a timeout, we implicitly need WAIT */
				/* Check if user already set NOWAIT? If conflicting, timeout usually ignored or error.
				   Firebird: isc_tpb_wait is required for isc_tpb_lock_timeout.
				   If user didn't set resolution, we add isc_tpb_wait.
				   But we can't easily check if we already added it in the stream without parsing back or tracking state.
				   Let's assume smart usage or add isc_tpb_wait if not added?
				   Actually, duplications in TPB are usually fine or last one wins?
				   Let's just append isc_tpb_lock_timeout. */
				/* Actually, standard implementation in populate_trans handles it by nesting. */

				*p++ = isc_tpb_lock_timeout;
				*p++ = (unsigned char) sizeof(ISC_SHORT);
				/* VAX/Firebird little-endian order */
				*p++ = (unsigned char) (timeout & 0xff);
				*p++ = (unsigned char) ((timeout >> 8) & 0xff);
				*trans_timeout = timeout;
			}
		}
	}

	/* read_consistency (Firebird 4.0+) */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "read_consistency", sizeof("read_consistency") - 1)) != NULL) {
		if (zend_is_true(tmp)) {
			*p++ = isc_tpb_read_consistency;
			*p++ = 1;
		}
	}

	/* tables reservation (Firebird 1.5+) */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "tables", sizeof("tables") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_ARRAY) {
			zval *table_val;
			zend_string *table_name;

			ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(tmp), table_name, table_val) {
				if (table_name && Z_TYPE_P(table_val) == IS_LONG) {
					zend_long lock_mode = Z_LVAL_P(table_val);
					unsigned char tlen = (unsigned char) ZSTR_LEN(table_name);

					// Basic Direction (Read/Write)
					if (lock_mode & PHP_FBIRD_LOCK_WRITE) {
						if (p < end) *p++ = isc_tpb_lock_write;
					} else if (lock_mode & PHP_FBIRD_LOCK_READ) {
						if (p < end) *p++ = isc_tpb_lock_read;
					} else {
						// Default to WRITE if only mode bits are set (common assumption) or SKIP?
						// Let's skip if no direction is set to avoid invalid TPB
						continue;
					}

					// Table Name
					if (p + 1 + tlen < end) {
						*p++ = tlen;
						memcpy(p, ZSTR_VAL(table_name), tlen);
						p += tlen;
					} else {
						// Buffer overflow prevention
						php_error_docref(NULL, E_WARNING, "TPB buffer too small for table '%s'", ZSTR_VAL(table_name));
						break;
					}

					// Lock Mode (Shared/Protected/Exclusive)
					if (lock_mode & PHP_FBIRD_LOCK_PROTECTED) {
						if (p < end) *p++ = isc_tpb_protected;
					} else if (lock_mode & PHP_FBIRD_LOCK_EXCLUSIVE) {
						if (p < end) *p++ = isc_tpb_exclusive;
					} else if (lock_mode & PHP_FBIRD_LOCK_SHARED) {
						if (p < end) *p++ = isc_tpb_shared;
					} else {
						// Default to SHARED if no mode specified
						if (p < end) *p++ = isc_tpb_shared;
					}
				}
			} ZEND_HASH_FOREACH_END();
		}
	}

	*len = (unsigned short) (p - (unsigned char *) last_tpb);
}

PHP_FUNCTION(fbird_trans_start)
{
	zval *link_arg = NULL, *options_arg = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *ib_trans;
	char last_tpb[TPB_MAX_SIZE];
	unsigned short tpb_len = 0;
	zend_long trans_timeout = 0;

	RESET_ERRMSG;

	/* M3: "|z" so Firebird\Connection objects are accepted alongside resources */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|za", &link_arg, &options_arg) == FAILURE) {
		return;
	}

	/* If first arg is array, it's options, and use default link */
	if (link_arg && Z_TYPE_P(link_arg) == IS_ARRAY) {
		options_arg = link_arg;
		link_arg = NULL;
	}

	if (link_arg) {
		/* M3: object path for Firebird\Connection */
		if (Z_TYPE_P(link_arg) == IS_OBJECT &&
			instanceof_function(Z_OBJCE_P(link_arg), fbird_connection_ce)) {
			zend_resource *cres = fbird_connection_get_resource(Z_OBJ_P(link_arg));
			ib_link = cres ? (fbird_db_link *)cres->ptr : NULL;
		} else {
			ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
		}
	} else {
		ib_link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), LE_LINK, le_link, le_plink);
	}

	if (!ib_link) {
		php_error_docref(NULL, E_WARNING, "Invalid database link");
		RETURN_FALSE;
	}

	if (options_arg) {
		_php_fbird_populate_trans_from_array(options_arg, &trans_timeout, last_tpb, &tpb_len);
	} else {
		/* Default transaction parameters */
		zend_long trans_argl = IBG(default_trans_params);
		trans_timeout = IBG(default_lock_timeout);
		_php_fbird_populate_trans(trans_argl, trans_timeout, last_tpb, &tpb_len);
	}

	/* OO API Only: All connections use fbt_start() */
	if (ib_link->fbc_connection == NULL) {
		_php_fbird_module_error("Connection has no OO API handle");
		RETURN_FALSE;
	}

	void* attachment = fbc_get_attachment(ib_link->fbc_connection);
	if (attachment == NULL) {
		_php_fbird_module_error("Failed to get attachment from OO API connection");
		RETURN_FALSE;
	}

	ib_trans = (fbird_transaction *) safe_emalloc(1-1, sizeof(fbird_db_link *), sizeof(fbird_transaction));
	ib_trans->fbt_transaction = fbt_start(
		IBG(master_instance),
		attachment,
		tpb_len,
		tpb_len > 0 ? (const unsigned char*)last_tpb : NULL,
		IB_STATUS
	);

	if (ib_trans->fbt_transaction == NULL) {
		efree(ib_trans);
		_php_fbird_error();
		RETURN_FALSE;
	}

	ib_trans->link_cnt = 1;
	ib_trans->affected_rows = 0;
	ib_trans->db_link[0] = ib_link;

	/* the first item in the connection-transaction list is reserved for the default transaction */
	if (ib_link->tr_list == NULL) {
		ib_link->tr_list = (fbird_tr_list *) emalloc(sizeof(fbird_tr_list));
		ib_link->tr_list->trans = NULL;
		ib_link->tr_list->next = NULL;
	}

	/* link the transaction into the connection-transaction list */
	fbird_tr_list **l;
	for (l = &ib_link->tr_list; *l != NULL; l = &(*l)->next);
	*l = (fbird_tr_list *) emalloc(sizeof(fbird_tr_list));
	(*l)->trans = ib_trans;
	(*l)->next = NULL;

	zend_resource *res = zend_register_resource(ib_trans, le_trans);
	fbird_setup_transaction_object(return_value, res);
}

static void _php_fbird_exec_savepoint(INTERNAL_FUNCTION_PARAMETERS, const char *format)
{
	zval *trans_arg = NULL;
	char *name;
	size_t name_len;
	fbird_transaction *trans;
	char *query;
	int len;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &trans_arg, &name, &name_len)) {
		return;
	}

	if (name_len == 0 || name_len > 31) { // Max identifier length
		php_error_docref(NULL, E_WARNING, "Invalid savepoint name (length must be 1-31 bytes)");
		RETURN_FALSE;
	}

	zend_resource *sp_tres = _php_fbird_trans_res_from_zval(trans_arg);
	if (!sp_tres) {
		php_error_docref(NULL, E_WARNING, "Argument #1 must be a Firebird\\Transaction object or transaction resource");
		RETURN_FALSE;
	}
	trans = (fbird_transaction *)sp_tres->ptr;
	if (!trans) {
		RETURN_FALSE;
	}

	/* Check if transaction involves exactly one connection */
	if (trans->link_cnt > 1) {
		_php_fbird_module_error("Savepoints not supported for multi-database transactions");
		RETURN_FALSE;
	}

	len = spprintf(&query, 0, format, name);

	/* OO API Only: All transactions use fbs_prepare() + fbs_execute() */
	if (trans->fbt_transaction == NULL) {
		_php_fbird_module_error("Transaction has no OO API handle");
		efree(query);
		RETURN_FALSE;
	}

	fbird_db_link *link = trans->db_link[0];
	void *attachment = NULL;
	void *transaction_ptr = NULL;
	void *stmt = NULL;

	FBDEBUG("OO API: Executing savepoint statement via fbs_prepare/execute");

	/* Get attachment from connection */
	if (!link->fbc_connection) {
		_php_fbird_module_error("OO API transaction without OO API connection");
		efree(query);
		RETURN_FALSE;
	}
	attachment = fbc_get_attachment(link->fbc_connection);
	if (!attachment) {
		_php_fbird_module_error("Failed to get attachment from connection");
		efree(query);
		RETURN_FALSE;
	}

	/* Get transaction handle */
	transaction_ptr = fbt_get_handle(trans->fbt_transaction);
	if (!transaction_ptr) {
		_php_fbird_module_error("Failed to get transaction handle");
		efree(query);
		RETURN_FALSE;
	}

	/* Prepare the savepoint statement */
	stmt = fbs_prepare(IBG(master_instance), attachment, transaction_ptr,
		query, (unsigned)len, SQL_DIALECT_CURRENT, IB_STATUS);
	if (!stmt) {
		_php_fbird_error();
		efree(query);
		RETURN_FALSE;
	}

	/* Execute the savepoint statement (no input/output parameters) */
	if (!fbs_execute(IBG(master_instance), stmt, transaction_ptr,
			NULL, NULL, NULL, NULL, IB_STATUS)) {
		_php_fbird_error();
		fbs_free(stmt, IB_STATUS);
		efree(query);
		RETURN_FALSE;
	}

	/* Free the statement */
	fbs_free(stmt, IB_STATUS);
	efree(query);
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_savepoint)
{
	_php_fbird_exec_savepoint(INTERNAL_FUNCTION_PARAM_PASSTHRU, "SAVEPOINT %s");
}

PHP_FUNCTION(fbird_rollback_savepoint)
{
	_php_fbird_exec_savepoint(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ROLLBACK TO SAVEPOINT %s");
}

PHP_FUNCTION(fbird_release_savepoint)
{
	_php_fbird_exec_savepoint(INTERNAL_FUNCTION_PARAM_PASSTHRU, "RELEASE SAVEPOINT %s");
}

PHP_FUNCTION(fbird_trans_info)
{
	zval *trans_arg;
	fbird_transaction *trans;
	char tpb[] = {
		isc_info_tra_id,
		isc_info_tra_isolation,
		isc_info_tra_lock_timeout,
		isc_info_tra_access
	};
	char res_buf[128];
	char *p = res_buf;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "z", &trans_arg)) {
		RETURN_FALSE;
	}

	zend_resource *ti_res = _php_fbird_trans_res_from_zval(trans_arg);
	if (!ti_res) {
		php_error_docref(NULL, E_WARNING, "Argument #1 must be a Firebird\\Transaction object or transaction resource");
		RETURN_FALSE;
	}
	trans = (fbird_transaction *)ti_res->ptr;
	if (!trans) {
		RETURN_FALSE;
	}

	if (trans->fbt_transaction == NULL) {
		_php_fbird_module_error("Transaction has no valid OO API handle");
		RETURN_FALSE;
	}

	if (fbt_get_info(
			IBG(master_instance),
			fbt_get_handle(trans->fbt_transaction),
			sizeof(tpb),
			(const unsigned char*)tpb,
			sizeof(res_buf),
			(unsigned char*)res_buf,
			IB_STATUS
		) == 0) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	array_init(return_value);

	const char *end = res_buf + sizeof(res_buf);
	while (p < end && *p != isc_info_end) {
		unsigned char item = *p++;

		/* Bounds-check: need 2 bytes for the length field */
		if (p + 2 > end) {
			break;
		}
		unsigned short len = (unsigned short)isc_vax_integer(p, 2);
		p += 2;

		/* Bounds-check: need len bytes for the value field */
		if (p + len > end) {
			break;
		}

		switch (item) {
			case isc_info_tra_id:
				add_assoc_long(return_value, "id", isc_vax_integer(p, len));
				break;
			case isc_info_tra_isolation:
				if (len > 0) {
					unsigned char iso = *p;
					switch(iso) {
						case isc_info_tra_consistency: add_assoc_string(return_value, "isolation", "CONSISTENCY"); break;
						case isc_info_tra_concurrency: add_assoc_string(return_value, "isolation", "CONCURRENCY"); break;
						case isc_info_tra_read_committed: add_assoc_string(return_value, "isolation", "READ_COMMITTED"); break;
						default: add_assoc_long(return_value, "isolation_raw", iso); break;
					}
				}
				break;
			case isc_info_tra_lock_timeout:
				add_assoc_long(return_value, "lock_timeout", isc_vax_integer(p, len));
				break;
			case isc_info_tra_access:
				if (len > 0) {
					add_assoc_string(return_value, "access_mode", (*p == isc_info_tra_readonly) ? "READ_ONLY" : "READ_WRITE");
				}
				break;
			default:
				/* Ignore unknown info items */
				break;
		}
		p += len;
	}

	/* Add internal state tracking if possible, or just what API returned */
	/* Since we don't track STATE in struct, we infer it is ACTIVE if valid resource */
	add_assoc_string(return_value, "state", "ACTIVE");
}

PHP_FUNCTION(fbird_connection_info)
{
	zval *link_arg = NULL;
	fbird_db_link *ib_link;
	char info_items[] = {
		isc_info_reads,
		isc_info_writes,
		isc_info_fetches,
		isc_info_marks,
		isc_info_page_size,
		isc_info_num_buffers,
		isc_info_current_memory,
		isc_info_max_memory,
		isc_info_allocation,
		isc_info_attachment_id,
		isc_info_ods_version,
		isc_info_ods_minor_version,
		isc_info_db_sql_dialect,
		isc_info_end
	};
	char res_buf[512];
	char *p;

	RESET_ERRMSG;

	/* M3: "|z!" so Firebird\Connection objects are accepted alongside resources */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		return;
	}

	if (link_arg == NULL) {
		ib_link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), LE_LINK, le_link, le_plink);
	} else if (Z_TYPE_P(link_arg) == IS_OBJECT &&
		instanceof_function(Z_OBJCE_P(link_arg), fbird_connection_ce)) {
		/* M3: object path for Firebird\Connection */
		zend_resource *cres = fbird_connection_get_resource(Z_OBJ_P(link_arg));
		ib_link = cres ? (fbird_db_link *)cres->ptr : NULL;
	} else {
		ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
	}

	if (!ib_link) {
		php_error_docref(NULL, E_WARNING, "Invalid database link");
		RETURN_FALSE;
	}

	/* Use OO API for connections that have fbc_connection */
	if (ib_link->fbc_connection != NULL) {
		void* attachment = fbc_get_attachment(ib_link->fbc_connection);
		if (attachment == NULL) {
			_php_fbird_module_error("Failed to get attachment from OO API connection");
			RETURN_FALSE;
		}

		if (!fbc_get_info(IBG(master_instance), attachment,
				sizeof(info_items), (const unsigned char*)info_items,
				sizeof(res_buf), (unsigned char*)res_buf, IB_STATUS)) {
			_php_fbird_error();
			RETURN_FALSE;
		}
	} else {
		_php_fbird_module_error("No OO API connection available for database info");
		RETURN_FALSE;
	}

	array_init(return_value);
	p = res_buf;

	const char *end = res_buf + sizeof(res_buf);
	while (p < end && *p != isc_info_end) {
		unsigned char item = *p++;

		/* Bounds-check: need 2 bytes for the length field */
		if (p + 2 > end) {
			break;
		}
		unsigned short len = (unsigned short)isc_vax_integer(p, 2);
		p += 2;

		/* Bounds-check: need len bytes for the value field */
		if (p + len > end) {
			break;
		}

		switch (item) {
			case isc_info_reads:
				add_assoc_long(return_value, "reads", isc_vax_integer(p, len));
				break;
			case isc_info_writes:
				add_assoc_long(return_value, "writes", isc_vax_integer(p, len));
				break;
			case isc_info_fetches:
				add_assoc_long(return_value, "fetches", isc_vax_integer(p, len));
				break;
			case isc_info_marks:
				add_assoc_long(return_value, "marks", isc_vax_integer(p, len));
				break;
			case isc_info_page_size:
				add_assoc_long(return_value, "page_size", isc_vax_integer(p, len));
				break;
			case isc_info_num_buffers:
				add_assoc_long(return_value, "num_buffers", isc_vax_integer(p, len));
				break;
			case isc_info_current_memory:
				add_assoc_long(return_value, "current_memory", isc_vax_integer(p, len));
				break;
			case isc_info_max_memory:
				add_assoc_long(return_value, "max_memory", isc_vax_integer(p, len));
				break;
			case isc_info_allocation:
				add_assoc_long(return_value, "allocation", isc_vax_integer(p, len));
				break;
			case isc_info_attachment_id:
				add_assoc_long(return_value, "attachment_id", isc_vax_integer(p, len));
				break;
			case isc_info_ods_version:
				add_assoc_long(return_value, "ods_version", isc_vax_integer(p, len));
				break;
			case isc_info_ods_minor_version:
				add_assoc_long(return_value, "ods_minor_version", isc_vax_integer(p, len));
				break;
			case isc_info_db_sql_dialect:
				add_assoc_long(return_value, "sql_dialect", isc_vax_integer(p, len));
				break;
			case isc_info_truncated:
				add_assoc_bool(return_value, "truncated", 1);
				return;
			default:
				break;
		}
		p += len;
	}
}

PHP_FUNCTION(fbird_trans)
{
	int i, argn = ZEND_NUM_ARGS();
	unsigned short link_cnt = 0, tpb_len = 0;
	char last_tpb[TPB_MAX_SIZE];
	fbird_db_link **ib_link = NULL;
	fbird_transaction *ib_trans;

	RESET_ERRMSG;

	/* (1+argn) is an upper bound for the number of links this trans connects to */
	ib_link = (fbird_db_link **) safe_emalloc(sizeof(fbird_db_link *),1+argn,0);

	if (argn > 0) {
		zend_long trans_argl = 0;
		zend_long trans_timeout = 0;
		char *tpb;
		unsigned short link0_tpb_len = 0;  /* TPB len for first connection */
		zval *args = NULL;

		if (zend_parse_parameters(argn, "+", &args, &argn) == FAILURE) {
			efree(ib_link);
			RETURN_FALSE;
		}

		tpb = (char *) safe_emalloc(TPB_MAX_SIZE,argn,0);

		/* enumerate all the arguments: assume every non-resource argument
		   specifies modifiers for the link ids that follow it */
		for (i = 0; i < argn; ++i) {

			if (Z_TYPE(args[i]) == IS_RESOURCE) {

				if ((ib_link[link_cnt] = (fbird_db_link *)zend_fetch_resource2_ex(&args[i], LE_LINK, le_link, le_plink)) == NULL) {
					efree(tpb);
					efree(ib_link);
					RETURN_FALSE;
				}

				/* copy the most recent modifier string into tpb[] */
				memcpy(&tpb[TPB_MAX_SIZE * link_cnt], last_tpb, TPB_MAX_SIZE);

				/* save TPB length for this connection */
				if (link_cnt == 0) {
					link0_tpb_len = tpb_len;
				}

				++link_cnt;

			} else if (Z_TYPE(args[i]) == IS_OBJECT &&
					instanceof_function(Z_OBJCE(args[i]), fbird_connection_ce)) {
				/* Phase E: Accept Firebird\Connection objects as connection args.
				 * Extract the underlying le_link resource and fetch fbird_db_link*. */
				zend_resource *conn_res = fbird_connection_get_resource(Z_OBJ(args[i]));
				if (!conn_res) {
					efree(tpb);
					efree(ib_link);
					_php_fbird_module_error("Connection object has no valid resource");
					RETURN_FALSE;
				}
				if ((ib_link[link_cnt] = (fbird_db_link *)zend_fetch_resource2(conn_res, LE_LINK, le_link, le_plink)) == NULL) {
					efree(tpb);
					efree(ib_link);
					RETURN_FALSE;
				}

				/* copy the most recent modifier string into tpb[] */
				memcpy(&tpb[TPB_MAX_SIZE * link_cnt], last_tpb, TPB_MAX_SIZE);

				/* save TPB length for this connection */
				if (link_cnt == 0) {
					link0_tpb_len = tpb_len;
				}

				++link_cnt;

			} else {

				tpb_len = 0;

				convert_to_long_ex(&args[i]);
				trans_argl = Z_LVAL(args[i]);
				if (trans_argl != PHP_FBIRD_DEFAULT) {
					// Skip conflicting parameters
					if (PHP_FBIRD_NOWAIT != (trans_argl & PHP_FBIRD_NOWAIT) && PHP_FBIRD_WAIT == (trans_argl & PHP_FBIRD_WAIT)) {
						if (PHP_FBIRD_LOCK_TIMEOUT == (trans_argl & PHP_FBIRD_LOCK_TIMEOUT)) {
							if((i + 1 < argn) && (Z_TYPE(args[i + 1]) == IS_LONG)){
								i++;
								convert_to_long_ex(&args[i]);
								trans_timeout = Z_LVAL(args[i]);
							} else {
								php_error_docref(NULL, E_WARNING, "FBIRD_LOCK_TIMEOUT expects next argument to be timeout value");
							}
						}
					}
					_php_fbird_populate_trans(trans_argl, trans_timeout, last_tpb, &tpb_len);
				}
			}
		}

		if (link_cnt > 0) {
			/* OO API Only: Verify all connections have OO API handles */
			for (int j = 0; j < link_cnt; j++) {
				if (ib_link[j]->fbc_connection == NULL) {
					efree(tpb);
					efree(ib_link);
					_php_fbird_module_error("Connection %d has no OO API handle", j);
					RETURN_FALSE;
				}
			}

			if (link_cnt == 1) {
				/* Single OO API connection - use fbt_start */
				void* attachment = fbc_get_attachment(ib_link[0]->fbc_connection);
				if (attachment == NULL) {
					efree(tpb);
					efree(ib_link);
					_php_fbird_module_error("Failed to get attachment from OO API connection");
					RETURN_FALSE;
				}

				void* oo_trans = fbt_start(
					IBG(master_instance),
					attachment,
					link0_tpb_len,
					link0_tpb_len > 0 ? (const unsigned char*)tpb : NULL,
					IB_STATUS
				);

				if (oo_trans == NULL) {
					efree(tpb);
					efree(ib_link);
					_php_fbird_error();
					RETURN_FALSE;
				}

				/* Allocate and register transaction with OO API wrapper */
				ib_trans = (fbird_transaction *) safe_emalloc(link_cnt-1, sizeof(fbird_db_link *), sizeof(fbird_transaction));
				ib_trans->link_cnt = link_cnt;
				ib_trans->affected_rows = 0;
				ib_trans->fbt_transaction = oo_trans;

				efree(tpb);
				goto register_trans;
			} else {
				/* Multi-database OO API transactions not yet supported */
				efree(tpb);
				efree(ib_link);
				_php_fbird_module_error("Multi-database transactions with OO API connections not yet supported");
				RETURN_FALSE;
			}
		}

		efree(tpb);
	}

	if (link_cnt == 0) {
		link_cnt = 1;
		if ((ib_link[0] = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), LE_LINK, le_link, le_plink)) == NULL) {
			efree(ib_link);
			RETURN_FALSE;
		}
		/* OO API Only: All connections must have OO API handle */
		if (ib_link[0]->fbc_connection == NULL) {
			efree(ib_link);
			_php_fbird_module_error("Connection has no OO API handle");
			RETURN_FALSE;
		}

		void* attachment = fbc_get_attachment(ib_link[0]->fbc_connection);
		if (attachment == NULL) {
			efree(ib_link);
			_php_fbird_module_error("Failed to get attachment from OO API connection");
			RETURN_FALSE;
		}

		void* oo_trans = fbt_start(
			IBG(master_instance),
			attachment,
			tpb_len,
			tpb_len > 0 ? (const unsigned char*)last_tpb : NULL,
			IB_STATUS
		);
		if (oo_trans == NULL) {
			_php_fbird_error();
			efree(ib_link);
			RETURN_FALSE;
		}

		/* Allocate and register transaction with OO API wrapper */
		ib_trans = (fbird_transaction *) safe_emalloc(link_cnt-1, sizeof(fbird_db_link *), sizeof(fbird_transaction));
		ib_trans->link_cnt = link_cnt;
		ib_trans->affected_rows = 0;
		ib_trans->fbt_transaction = oo_trans;
	}

register_trans:
	for (i = 0; i < link_cnt; ++i) {
		fbird_tr_list **l;
		ib_trans->db_link[i] = ib_link[i];

		/* the first item in the connection-transaction list is reserved for the default transaction */
		if (ib_link[i]->tr_list == NULL) {
			ib_link[i]->tr_list = (fbird_tr_list *) emalloc(sizeof(fbird_tr_list));
			ib_link[i]->tr_list->trans = NULL;
			ib_link[i]->tr_list->next = NULL;
		}

		/* link the transaction into the connection-transaction list */
		for (l = &ib_link[i]->tr_list; *l != NULL; l = &(*l)->next);
		*l = (fbird_tr_list *) emalloc(sizeof(fbird_tr_list));
		(*l)->trans = ib_trans;
		(*l)->next = NULL;
	}
	efree(ib_link);
	zend_resource *res = zend_register_resource(ib_trans, le_trans);
	fbird_setup_transaction_object(return_value, res);
}

int _php_fbird_def_trans(fbird_db_link *ib_link, fbird_transaction **trans)
{
	if (ib_link == NULL) {
		php_error_docref(NULL, E_WARNING, "Invalid database link");
		return FAILURE;
	}

	/* the first item in the connection-transaction list is reserved for the default transaction */
	if (ib_link->tr_list == NULL) {
		ib_link->tr_list = (fbird_tr_list *) emalloc(sizeof(fbird_tr_list));
		ib_link->tr_list->trans = NULL;
		ib_link->tr_list->next = NULL;
	}

	if (*trans == NULL) {
		fbird_transaction *tr = ib_link->tr_list->trans;

		if (tr == NULL) {
			tr = (fbird_transaction *) emalloc(sizeof(fbird_transaction));
			tr->link_cnt = 1;
			tr->affected_rows = 0;
			tr->fbt_transaction = NULL;
			tr->db_link[0] = ib_link;
			ib_link->tr_list->trans = tr;
		}

		if (tr->fbt_transaction == NULL) {
			zend_long trans_argl = IBG(default_trans_params);
			char last_tpb[TPB_MAX_SIZE];
			unsigned short tpb_len = 0;

			if (trans_argl != PHP_FBIRD_DEFAULT) {
				zend_long trans_timeout = IBG(default_lock_timeout);
				_php_fbird_populate_trans(trans_argl, trans_timeout, last_tpb, &tpb_len);
			}

			if (ib_link->fbc_connection == NULL) {
				_php_fbird_module_error("Connection has no OO API handle");
				return FAILURE;
			}

			void* attachment = fbc_get_attachment(ib_link->fbc_connection);
			if (attachment == NULL) {
				_php_fbird_module_error("Failed to get attachment from OO API connection");
				return FAILURE;
			}

			tr->fbt_transaction = fbt_start(
				IBG(master_instance),
				attachment,
				tpb_len,
				tpb_len > 0 ? (const unsigned char*)last_tpb : NULL,
				IB_STATUS
			);

			if (tr->fbt_transaction == NULL) {
				_php_fbird_error();
				return FAILURE;
			}
		}
		*trans = tr;
	}
	return SUCCESS;
}

static void _php_fbird_trans_end(INTERNAL_FUNCTION_PARAMETERS, int commit)
{
	fbird_transaction *trans = NULL;
	int res_id = 0;
	ISC_STATUS result;
	fbird_db_link *ib_link;
	zval *arg = NULL;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &arg) == FAILURE) {
		return;
	}

	if (ZEND_NUM_ARGS() == 0) {
		ib_link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), LE_LINK, le_link, le_plink);
		if (ib_link->tr_list == NULL || ib_link->tr_list->trans == NULL) {
			/* this link doesn't have a default transaction */
			_php_fbird_module_error("Default link has no default transaction");
			RETURN_FALSE;
		}
		trans = ib_link->tr_list->trans;
	} else {
		/* one id was passed - could be Firebird\Transaction object, le_trans resource, or db link */
		ZVAL_DEREF(arg);
		if (Z_TYPE_P(arg) == IS_OBJECT &&
				instanceof_function(Z_OBJCE_P(arg), fbird_transaction_ce)) {
			/* Firebird\Transaction object path */
			zend_resource *tres = fbird_transaction_get_resource(Z_OBJ_P(arg));
			if (!tres) {
				_php_fbird_module_error("Firebird\\Transaction object has no active resource");
				RETURN_FALSE;
			}
			trans = (fbird_transaction *)tres->ptr;
			res_id = tres->handle;
			arg = NULL; /* signal: don't use Z_RES_P(arg) for zend_list_delete below */
		} else if (Z_TYPE_P(arg) == IS_RESOURCE &&
				   Z_RES_P(arg)->type == le_trans) {
			trans = (fbird_transaction *)zend_fetch_resource_ex(arg, LE_TRANS, le_trans);
			res_id = Z_RES_P(arg)->handle;
		} else if (Z_TYPE_P(arg) == IS_OBJECT &&
				instanceof_function(Z_OBJCE_P(arg), fbird_connection_ce)) {
			/* M3: Firebird\Connection object — get underlying resource then default trans */
			zend_resource *cres = fbird_connection_get_resource(Z_OBJ_P(arg));
			if (!cres || !cres->ptr) {
				_php_fbird_module_error("Firebird\\Connection object has no valid resource");
				RETURN_FALSE;
			}
			ib_link = (fbird_db_link *)cres->ptr;
			if (ib_link->tr_list == NULL || ib_link->tr_list->trans == NULL) {
				_php_fbird_module_error("Firebird\\Connection object has no default transaction");
				RETURN_FALSE;
			}
			trans = ib_link->tr_list->trans;
			arg = NULL; /* prevent Z_RES_P(arg) usage below */
		} else {
			ib_link = (fbird_db_link *)zend_fetch_resource2_ex(arg, LE_LINK, le_link, le_plink);

			if (!ib_link || ib_link->tr_list == NULL || ib_link->tr_list->trans == NULL) {
				/* this link doesn't have a default transaction */
				_php_fbird_module_error("Link has no default transaction");
				RETURN_FALSE;
			}
			trans = ib_link->tr_list->trans;
		}
	}

	/* OO API Only: All transactions use fbt_* functions */
	if (trans->fbt_transaction == NULL) {
		_php_fbird_module_error("invalid transaction handle (expecting explicit transaction start) ");
		RETURN_FALSE;
	}

	switch (commit) {
		default: /* == case ROLLBACK: */
			result = fbt_rollback(trans->fbt_transaction, IB_STATUS);
			break;
		case COMMIT:
			result = fbt_commit(trans->fbt_transaction, IB_STATUS);
			break;
		case (ROLLBACK | RETAIN):
			result = fbt_rollback_retaining(trans->fbt_transaction, IB_STATUS);
			break;
		case (COMMIT | RETAIN):
			result = fbt_commit_retaining(trans->fbt_transaction, IB_STATUS);
			break;
	}

	/* Clear handle for non-retained operations BEFORE checking result.
	 * The fbt_* functions ALWAYS delete the wrapper (even on error),
	 * so we must clear our pointer to avoid dangling references.
	 * Fixes: #9, #10 - SIGSEGV due to use-after-free of transaction wrapper
	 */
	if ((commit & RETAIN) == 0) {
		fbt_free(trans->fbt_transaction);
		trans->fbt_transaction = NULL;
	}

	if (result) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* Don't try to destroy implicitly opened transaction from list...
	 * If arg was NULLed (object path), skip Z_RES_P - resource auto-freed via
	 * trans->fbt_transaction=NULL above prevents double-free in destructor. */
	if ((commit & RETAIN) == 0 && res_id != 0 && arg != NULL) {
		zend_list_delete(Z_RES_P(arg));
	}
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_commit)
{
	_php_fbird_trans_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, COMMIT);
}

PHP_FUNCTION(fbird_rollback)
{
	_php_fbird_trans_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, ROLLBACK);
}

PHP_FUNCTION(fbird_commit_ret)
{
	_php_fbird_trans_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, COMMIT | RETAIN);
}

PHP_FUNCTION(fbird_rollback_ret)
{
	_php_fbird_trans_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, ROLLBACK | RETAIN);
}

int is_valid_identifier(const char *s, size_t len)
{
	size_t i;
	if (len == 0) return 0;

	if (s[0] == '"') {
		/* Quoted identifier: must end with quote, and internal quotes must be paired */
		if (len < 2 || s[len-1] != '"') return 0;
		for (i = 1; i < len - 1; i++) {
			if (s[i] == '"') {
				if (s[i+1] == '"') {
					i++; /* skip paired quote */
				} else {
					return 0; /* unescaped quote in middle */
				}
			}
		}
		return 1;
	}

	/* Unquoted identifier: alphanumeric, _, $ */
	for (i = 0; i < len; i++) {
		char c = s[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			  (c >= '0' && c <= '9') || c == '_' || c == '$')) {
			return 0;
		}
	}
	return 1;
}

#endif /* HAVE_FIREBIRD */

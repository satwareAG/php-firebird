/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_FIREBIRD

#include "php_ini.h"
#include "ext/standard/md5.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_connection.h"
#include "firebird_utils.h"
#include "fbird_classes.h"

/* Fill ib_link and trans with the correct database link and transaction.
 * M3: Accepts both legacy zend_resource zvals and Firebird\Connection /
 * Firebird\Transaction objects (weak-ref to the same internal resource). */
void _php_fbird_get_link_trans(INTERNAL_FUNCTION_PARAMETERS,
	zval *link_id, fbird_db_link **ib_link, fbird_transaction **trans)
{
	FBDEBUG("Transaction or database link?");

	/* Object path: Firebird\Transaction or Firebird\Connection */
	if (Z_TYPE_P(link_id) == IS_OBJECT) {
		if (instanceof_function(Z_OBJCE_P(link_id), fbird_transaction_ce)) {
			FBDEBUG("IS_OBJECT: Firebird\\Transaction");
			zend_resource *tres = fbird_transaction_get_resource(Z_OBJ_P(link_id));
			if (!tres) {
				_php_fbird_module_error("Invalid Firebird\\Transaction object");
				return;
			}
			*trans = (fbird_transaction *)tres->ptr;
			if ((*trans)->link_cnt > 1) {
				_php_fbird_module_error("Link id is ambiguous: transaction spans multiple connections.");
				return;
			}
			*ib_link = (*trans)->db_link[0];
			return;
		} else if (instanceof_function(Z_OBJCE_P(link_id), fbird_connection_ce)) {
			FBDEBUG("IS_OBJECT: Firebird\\Connection");
			zend_resource *cres = fbird_connection_get_resource(Z_OBJ_P(link_id));
			if (!cres) {
				_php_fbird_module_error("Invalid Firebird\\Connection object");
				return;
			}
			*trans = NULL;
			*ib_link = (fbird_db_link *)cres->ptr;
			return;
		}
	}

	/* Resource path: legacy le_trans or le_link/le_plink */
	if (Z_TYPE_P(link_id) == IS_RESOURCE && Z_RES_P(link_id)->type == le_trans) {
		/* Transaction resource: make sure it refers to one link only, then
		   fetch it; database link is stored in ib_trans->db_link[]. */
		FBDEBUG("IS_RESOURCE: le_trans");
		*trans = (fbird_transaction *)zend_fetch_resource_ex(link_id, LE_TRANS, le_trans);
		if ((*trans)->link_cnt > 1) {
			_php_fbird_module_error("Link id is ambiguous: transaction spans multiple connections.");
			return;
		}
		*ib_link = (*trans)->db_link[0];
		return;
	}
	FBDEBUG("IS_RESOURCE: le_[p]link or id not found");
	/* Database link resource, use default transaction. */
	*trans = NULL;
	*ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_id, LE_LINK, le_link, le_plink);
}

/* destructors ---------------------- */

void _php_fbird_commit_link(fbird_db_link *link)
{
	unsigned short i = 0, j;
	fbird_tr_list *l;
	fbird_event *e;
	FBDEBUG("Checking transactions to close...");

	for (l = link->tr_list; l != NULL; ++i) {
		fbird_tr_list *p = l;
		if (p->trans != 0) {
			if (i == 0) {
				/* Default transaction: commit via OO API */
				if (p->trans->fbt_transaction != NULL) {
					FBDEBUG("Committing default transaction via OO API...");
					int res = fbt_commit(p->trans->fbt_transaction, IB_STATUS);
					fbt_free(p->trans->fbt_transaction);
					p->trans->fbt_transaction = NULL;
					/* Guard error reporting during MSHUTDOWN (Issue #183).
					 * _php_fbird_error() accesses EG() globals which may be
					 * destroyed during persistent connection cleanup. */
					if (res && !IBG(in_mshutdown)) {
						_php_fbird_error();
					}
				}
				efree(p->trans); /* default transaction is not a registered resource: clean up */
			} else {
				/* Non-default transaction: rollback via OO API */
				if (p->trans->fbt_transaction != NULL) {
					FBDEBUG("Rolling back other transaction via OO API...");
					int res = fbt_rollback(p->trans->fbt_transaction, IB_STATUS);
					fbt_free(p->trans->fbt_transaction);
					p->trans->fbt_transaction = NULL;
					if (res && !IBG(in_mshutdown)) {
						_php_fbird_error();
					}
				}
				/* set this link pointer to NULL in the transaction */
				for (j = 0; j < p->trans->link_cnt; ++j) {
					if (p->trans->db_link[j] == link) {
						p->trans->db_link[j] = NULL;
						break;
					}
				}
			}
		}
		l = l->next;
		efree(p);
	}
	link->tr_list = NULL;

	/*
	 * IMPORTANT: _php_fbird_free_event() unlinks and efree()s the current node.
	 * Never follow e->event_next after freeing e.
	 */
	for (e = link->event_head; e; ) {
		fbird_event *next = e->event_next;
		_php_fbird_free_event(e);
		/* e is freed inside _php_fbird_free_event(); only use cached next */
		e = next;
	}
}


void php_fbird_commit_link_rsrc(zend_resource *rsrc)
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	_php_fbird_commit_link(link);
}

void _php_fbird_close_link(zend_resource *rsrc)
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	/* NULL pointer guard (Issue #55): In forked PHPStan workers, rsrc->ptr may be NULL
	 * when inherited resource descriptors are destroyed during child process shutdown.
	 * Accessing link->created_pid with NULL pointer causes SIGSEGV at si_addr=0x4. */
	if (link == NULL) {
		return;
	}

	/* Clear default_link if this resource IS the default link (Issue #183, #184).
	 * Without this, IBG(default_link) becomes a dangling pointer after the link
	 * is freed, causing SIGSEGV when doctrine's TransactionManager later tries to
	 * use the default link via procedural API paths. */
	if (!IBG(in_mshutdown) && IBG(default_link) == rsrc) {
		IBG(default_link) = NULL;
	}

#ifndef PHP_WIN32
	/* Fork-safety check (Issue #22, #36): Skip cleanup if we're in a forked child.
	 * After pcntl_fork(), child inherits global state including master_instance
	 * and connection handles. Attempting to close handles in child that were
	 * created in parent causes segfault. Only the original process should
	 * perform cleanup operations.
	 *
	 * Two-level check:
	 * 1. Global init_pid - module-level fork detection
	 * 2. Per-connection created_pid - connection-level fork detection */
	pid_t current_pid = getpid();
	if (IBG(init_pid) != 0 && current_pid != IBG(init_pid)) {
		FBDEBUG("Skipping link cleanup in forked child process (global)");
		IBG(num_links)--;
		efree(link);
		return;
	}
	if (link->created_pid != 0 && current_pid != link->created_pid) {
		FBDEBUG("Skipping link cleanup in forked child process (per-connection)");
		IBG(num_links)--;
		efree(link);
		return;
	}
#endif

	/* Remove cache entry from EG(regular_list) to prevent UAF (Issue #35).
	 *
	 * CRITICAL: Skip EG() access during MSHUTDOWN (Issue #50, #51, #55).
	 * During module shutdown, EG(regular_list) may already be destroyed. */
	if (!IBG(in_mshutdown) &&
		(link->hash_key[0] != '\0' || memcmp(link->hash_key, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0)) {
		zend_hash_str_del(&EG(regular_list), link->hash_key, sizeof(link->hash_key) - 1);
		FBDEBUG("Removed cache entry for normal link");
	}

	_php_fbird_commit_link(link);

	/* OO API Only: All connections use fbc_disconnect() */
	if (link->fbc_connection != NULL) {
		FBDEBUG("Closing normal link via OO API...");
		fbc_disconnect(link->fbc_connection, IB_STATUS);
		link->fbc_connection = NULL;
	}
	IBG(num_links)--;
	efree(link);
}

void _php_fbird_close_plink(zend_resource *rsrc)
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	/* NULL pointer guard (Issue #55): In forked PHPStan workers, rsrc->ptr may be NULL
	 * when inherited resource descriptors are destroyed during child process shutdown.
	 * Accessing link->created_pid with NULL pointer causes SIGSEGV at si_addr=0x4. */
	if (link == NULL) {
		return;
	}

#ifndef PHP_WIN32
	/* Fork-safety check (Issue #22, #36): Skip cleanup if we're in a forked child.
	 * Two-level check for both module-level and connection-level fork detection. */
	pid_t current_pid = getpid();
	if (IBG(init_pid) != 0 && current_pid != IBG(init_pid)) {
		FBDEBUG("Skipping persistent link cleanup in forked child process (global)");
		IBG(num_persistent)--;
		IBG(num_links)--;
		free(link);
		return;
	}
	if (link->created_pid != 0 && current_pid != link->created_pid) {
		FBDEBUG("Skipping persistent link cleanup in forked child process (per-connection)");
		IBG(num_persistent)--;
		IBG(num_links)--;
		free(link);
		return;
	}
#endif

	/* Remove cache entries from both regular and persistent lists (Issue #35).
	 * Persistent connections are cached in EG(persistent_list) with hash key.
	 *
	 * CRITICAL: Skip EG() access during MSHUTDOWN (Issue #50, #51).
	 * During module shutdown, EG(regular_list) and EG(persistent_list) may already
	 * be destroyed, causing SIGSEGV (exit code 139) if accessed. */
	if (!IBG(in_mshutdown) &&
		(link->hash_key[0] != '\0' || memcmp(link->hash_key, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) != 0)) {
		zend_hash_str_del(&EG(regular_list), link->hash_key, sizeof(link->hash_key) - 1);
		zend_hash_str_del(&EG(persistent_list), link->hash_key, sizeof(link->hash_key) - 1);
		FBDEBUG("Removed cache entries for persistent link");
	}

	_php_fbird_commit_link(link);

	/* OO API Only: All connections use fbc_disconnect() */
	if (link->fbc_connection != NULL) {
		FBDEBUG("Closing permanent link via OO API...");
		fbc_disconnect(link->fbc_connection, IB_STATUS);
		link->fbc_connection = NULL;
	}
	IBG(num_persistent)--;
	IBG(num_links)--;
	free(link);
}

enum connect_args { DB = 0, USER = 1, PASS = 2, CSET = 3, ROLE = 4, BUF = 0, DLECT = 1, SYNC = 2 };

static char const dpb_args[] = {
	0, isc_dpb_user_name, isc_dpb_password, isc_dpb_lc_ctype, isc_dpb_sql_role_name, 0
};

int _php_fbird_attach_db(char **args, size_t *len, zend_long *largs, void **out_connection)
{
    void* connection = NULL;

    /* Use OO API as the connection method */
    connection = fbc_connect(
        IBG(master_instance),
        args[DB], len[DB],                          /* database path */
        args[USER], len[USER],                      /* username */
        args[PASS], len[PASS],                      /* password */
        args[CSET], len[CSET],                      /* charset */
        args[ROLE], len[ROLE],                      /* SQL role */
        (int)largs[BUF],                            /* num_buffers */
        largs[DLECT] ? (int)largs[DLECT] : SQL_DIALECT_CURRENT, /* dialect */
        (int)largs[SYNC],                           /* force_write */
        IB_STATUS                                   /* status vector */
    );

    if (!connection) {
        _php_fbird_error();
        return FAILURE;
    }

    /* Return connection pointer directly via output parameter */
    *out_connection = connection;

    return SUCCESS;
}

/**
 * Core connection logic extracted from _php_fbird_connect.
 * Accepts plain C arguments (already parsed/defaulted by caller).
 * Returns the new zend_resource* with appropriate refcount adjustments,
 * or NULL on failure (error already set via _php_fbird_error()).
 * Also applies INI-based defaults for empty args and manages IBG(default_link).
 */
zend_resource *_php_fbird_connect_link(
	char *db,      size_t db_len,
	char *user,    size_t user_len,
	char *pass,    size_t pass_len,
	char *charset, size_t charset_len,
	zend_long buffers, zend_long dialect,
	char *role,    size_t role_len,
	zend_long flags, int persistent)
{
	char *c, hash[16], *args[] = { db, user, pass, charset, role };
	int i;
	size_t len[] = { db_len, user_len, pass_len, charset_len, role_len };
	zend_long largs[] = { buffers, dialect, 0 };
	PHP_MD5_CTX hash_context;
	zend_resource new_index_ptr, *le;
	void *connection_ptr = NULL;
	fbird_db_link *ib_link;
	zend_resource *result_res = NULL;

	/* Apply INI-based defaults for empty args */
	if (!len[DB] && (c = INI_STR("fbird.default_db"))) {
		args[DB] = c;
		len[DB] = strlen(c);
	}
	if (!len[USER] && (c = INI_STR("fbird.default_user"))) {
		args[USER] = c;
		len[USER] = strlen(c);
	}
	if (!len[PASS] && (c = INI_STR("fbird.default_password"))) {
		args[PASS] = c;
		len[PASS] = strlen(c);
	}
	if (!len[CSET] && (c = INI_STR("fbird.default_charset"))) {
		args[CSET] = c;
		len[CSET] = strlen(c);
	}

	/* don't want usernames and passwords floating around */
	PHP_MD5Init(&hash_context);
	for (i = 0; i < sizeof(args)/sizeof(char*); ++i) {
		PHP_MD5Update(&hash_context,args[i],len[i]);
	}
	for (i = 0; i < sizeof(largs)/sizeof(zend_long); ++i) {
		PHP_MD5Update(&hash_context,(char*)&largs[i],sizeof(zend_long));
	}
	PHP_MD5Final((unsigned char*)hash, &hash_context);

	/* try to reuse a connection (skip if FBIRD_CONNECT_FORCE_NEW is set) */
	if (!(flags & PHP_FBIRD_CONNECT_FORCE_NEW) &&
			(le = zend_hash_str_find_ptr(&EG(regular_list), hash, sizeof(hash)-1)) != NULL) {
		zend_resource *xlink;

		if (le->type != le_index_ptr) {
			return NULL;
		}

		xlink = (zend_resource*) le->ptr;
		if ((!persistent && xlink->type == le_link) || xlink->type == le_plink) {
			/* Issue #119: Verify the cached connection is still usable.
			 * After fbird_close(), fbc_connection may be NULL even though
			 * the zend_resource still exists with refcount > 0. */
			ib_link = (fbird_db_link *)xlink->ptr;
			if (ib_link == NULL || ib_link->fbc_connection == NULL ||
				!fbc_is_connected(ib_link->fbc_connection)) {
				/* Stale cache entry — remove and fall through to create new connection */
				zend_hash_str_del(&EG(regular_list), hash, sizeof(hash)-1);
			} else {
				if (IBG(default_link) != xlink) {
					GC_ADDREF(xlink);
					if (IBG(default_link)) {
						zend_list_delete(IBG(default_link));
					}
					IBG(default_link) = xlink;
				}
				GC_ADDREF(xlink);
				return xlink;
			}
		} else {
			zend_hash_str_del(&EG(regular_list), hash, sizeof(hash)-1);
		}
	}

	/* ... or a persistent one */
	do {
		zend_long l;

		if ((le = zend_hash_str_find_ptr(&EG(persistent_list), hash, sizeof(hash)-1)) != NULL) {
			if (le->type != le_plink) {
				return NULL;
			}
			/* check if connection has timed out */
			ib_link = (fbird_db_link *) le->ptr;
			if (ib_link->fbc_connection && fbc_is_connected(ib_link->fbc_connection)) {
				result_res = zend_register_resource(ib_link, le_plink);
				break;
			}
			zend_hash_str_del(&EG(persistent_list), hash, sizeof(hash)-1);
		}

		/* no link found, so we have to open one */

		if ((l = INI_INT("fbird.max_links")) != -1 && IBG(num_links) >= l) {
			_php_fbird_module_error("Too many open links (%ld)", IBG(num_links));
			return NULL;
		}

		/* create the ib_link */
		if (FAILURE == _php_fbird_attach_db(args, len, largs, &connection_ptr)) {
			return NULL;
		}

		/* use non-persistent if allowed number of persistent links is exceeded */
		if (!persistent || ((l = INI_INT("fbird.max_persistent") != -1) && IBG(num_persistent) >= l)) {
			ib_link = (fbird_db_link *) emalloc(sizeof(fbird_db_link));
			result_res = zend_register_resource(ib_link, le_link);
		} else {
			ib_link = (fbird_db_link *) malloc(sizeof(fbird_db_link));
			if (!ib_link) {
				return NULL;
			}

			/* hash it up */
			if (zend_register_persistent_resource(hash, sizeof(hash)-1, ib_link, le_plink) == NULL) {
				free(ib_link);
				return NULL;
			}
			result_res = zend_register_resource(ib_link, le_plink);
			++IBG(num_persistent);
		}
		ib_link->dialect = largs[DLECT] ? (unsigned short)largs[DLECT] : SQL_DIALECT_CURRENT;
		ib_link->tr_list = NULL;
		ib_link->event_head = NULL;

		ib_link->fbc_connection = connection_ptr;

		/* Store hash key for cache invalidation on close (Issue #35) */
		memcpy(ib_link->hash_key, hash, sizeof(hash));

		/* Store creation PID for fork-safety detection (Issue #36) */
#ifndef PHP_WIN32
		ib_link->created_pid = getpid();
#else
		ib_link->created_pid = 0;
#endif

		++IBG(num_links);
	} while (0);

	/* add it to the hash */
	new_index_ptr.ptr = (void *) result_res;
	new_index_ptr.type = le_index_ptr;
	zend_hash_str_update_mem(&EG(regular_list), hash, sizeof(hash)-1,
			(void *) &new_index_ptr, sizeof(zend_resource));
	if (IBG(default_link)) {
		zend_list_delete(IBG(default_link));
	}
	IBG(default_link) = result_res;
	GC_ADDREF(result_res);  /* default_link ref */
	GC_ADDREF(result_res);  /* caller ref */
	return result_res;
}

void _php_fbird_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
	char *args_db = NULL, *args_user = NULL, *args_pass = NULL;
	char *args_cset = NULL, *args_role = NULL;
	size_t len_db = 0, len_user = 0, len_pass = 0, len_cset = 0, len_role = 0;
	zend_long buf = 0, dlect = 0, sync = 0, flags = 0;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|ssssllsll",
			&args_db, &len_db, &args_user, &len_user, &args_pass, &len_pass,
			&args_cset, &len_cset, &buf, &dlect, &args_role, &len_role,
			&sync, &flags)) {
		RETURN_FALSE;
	}

	zend_resource *res = _php_fbird_connect_link(
		args_db,   len_db,
		args_user, len_user,
		args_pass, len_pass,
		args_cset, len_cset,
		buf, dlect,
		args_role, len_role,
		flags, persistent);
	if (!res) {
		RETURN_FALSE;
	}
	/* Phase C: return Firebird\Connection object instead of raw resource.
	 * fbird_setup_connection_object stores a weak ref; default_link owns the resource.
	 * Release the "caller ref" that _php_fbird_connect_link added. */
	fbird_setup_connection_object(return_value, res);
	GC_DELREF(res);
}

PHP_FUNCTION(fbird_connect)
{
	_php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}

PHP_FUNCTION(fbird_pconnect)
{
	_php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, INI_INT("fbird.allow_persistent"));
}

/* Helper function for consolidated resource validation with proper error differentiation */
static int _php_fbird_validate_link_resource(zend_resource *link_res, bool is_default_link, bool clear_default)
{
	if (link_res == NULL) {
		return FAILURE;
	}

	/* Check resource type and validity directly */
	if (link_res->type != le_link && link_res->type != le_plink) {
		/* Wrong type - will be caught by zend_parse_parameters */
		return FAILURE;
	}

	/* Check if resource pointer is valid */
	if (link_res->ptr == NULL) {
		/* Correct resource type but invalid/closed - generate warning */
		php_error_docref(NULL, E_WARNING, "Supplied resource is not a valid database link resource");
		if (clear_default && is_default_link) {
			/* Thread-safe: Only clear if we were the default */
			if (IBG(default_link) == link_res) {
				IBG(default_link) = NULL;
			}
		}
		return FAILURE;
	}

	return SUCCESS;
}

/* Helper function for thread-safe default link adoption */
static void _php_fbird_adopt_new_default_link(zend_resource *closing_link)
{
	/* Only search if we're actually clearing the current default */
	if (IBG(default_link) != closing_link) {
		return;
	}

	/* For now, simply clear the default. Full adoption logic can be added in future enhancement.
	 * This maintains existing behavior while providing the infrastructure for adoption. */
	IBG(default_link) = NULL;
}

/* Helper function for optimized resource cleanup */
static void _php_fbird_close_resource(zend_resource *link_res)
{
	/* For persistent connections, check reference count more carefully */
	if (link_res->type == le_plink && GC_REFCOUNT(link_res) > 1) {
		/* Multiple references exist - just decrease our refcount */
		zend_list_delete(link_res);
	} else {
		/* Safe to close: either non-persistent or no other references */
		zend_list_close(link_res);
	}
}

/* Helper: extract zend_resource* from either a resource zval or a Firebird\Connection object.
 * Returns NULL if the zval is neither. */
static zend_resource *_php_fbird_res_from_zval(zval *zv)
{
	if (zv == NULL) return NULL;
	ZVAL_DEREF(zv);
	if (Z_TYPE_P(zv) == IS_RESOURCE) {
		return Z_RES_P(zv);
	}
	if (Z_TYPE_P(zv) == IS_OBJECT &&
		instanceof_function(Z_OBJCE_P(zv), fbird_connection_ce)) {
		/* Extract the connection resource from the Firebird\Connection object */
		return fbird_connection_get_resource(Z_OBJ_P(zv));
	}
	return NULL;
}

PHP_FUNCTION(fbird_close)
{
	zval *link_arg = NULL;
	zend_resource *link_res;
	bool is_default_link = false;

	RESET_ERRMSG;

	/* Accept resource OR Firebird\Connection object (Issue #120) */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		return;
	}

	/* Type enforcement: only resource, Firebird\Connection object, or null accepted */
	if (link_arg != NULL && Z_TYPE_P(link_arg) != IS_RESOURCE && Z_TYPE_P(link_arg) != IS_OBJECT) {
		zend_argument_type_error(1, "must be of type resource or null, %s given", zend_get_type_by_const(Z_TYPE_P(link_arg)));
		RETURN_THROWS();
	}

	/* Determine which link to close */
	if (ZEND_NUM_ARGS() == 0 || link_arg == NULL) {
		/* Default link path */
		link_res = IBG(default_link);
		is_default_link = true;

		if (link_res == NULL) {
			RETURN_FALSE;
		}
	} else {
		/* Explicit link path: accept resource or Connection object */
		link_res = _php_fbird_res_from_zval(link_arg);
		if (link_res == NULL) {
			RETURN_FALSE;
		}
		is_default_link = (IBG(default_link) == link_res);
	}

	/* Single validation point - handles all validation efficiently */
	if (_php_fbird_validate_link_resource(link_res, is_default_link, true) == FAILURE) {
		RETURN_FALSE;
	}

	/* Handle default link management BEFORE closing resource
	 * Special handling for explicit links that are also the default */
	if (is_default_link) {
		if (link_arg != NULL) {
			/* When closing explicit link that's also default, only clear if
			 * resource's reference count will drop to zero */
			if (GC_REFCOUNT(link_res) <= 2) {
				_php_fbird_adopt_new_default_link(link_res);
			}
		} else {
			/* Default link path - always clear default */
			_php_fbird_adopt_new_default_link(link_res);
		}
	}

	/* Optimized resource cleanup */
	_php_fbird_close_resource(link_res);

	RETURN_TRUE;
}

/* {{{ _php_fbird_escape_single_quotes
   Double every single-quote character in str to produce SQL-safe '' escaping.
   Returns a new zend_string; caller must zend_string_release(). */
static zend_string *_php_fbird_escape_single_quotes(const char *str, size_t len)
{
	/* Worst case: every character is a quote */
	zend_string *result = zend_string_alloc(len * 2, 0);
	char *dst = ZSTR_VAL(result);
	size_t dst_len = 0;

	for (size_t i = 0; i < len; i++) {
		if (str[i] == '\'') {
			dst[dst_len++] = '\'';
			dst[dst_len++] = '\'';
		} else {
			dst[dst_len++] = str[i];
		}
	}
	dst[dst_len] = '\0';
	ZSTR_LEN(result) = dst_len;
	return result;
}
/* }}} */

/* Firebird character set allowlist for CREATE DATABASE.
   Validated against Firebird 3.0/4.0/5.0 supported character sets. */
static const char *valid_charsets[] = {
	"NONE", "ASCII", "BIG_5", "CYRL", "DOS437", "DOS850", "DOS852",
	"DOS857", "DOS860", "DOS861", "DOS863", "DOS865", "EUCJ_0208",
	"GB_2312", "ISO8859_1", "ISO8859_2", "ISO8859_3", "ISO8859_4",
	"ISO8859_5", "ISO8859_6", "ISO8859_7", "ISO8859_8", "ISO8859_9",
	"ISO8859_13", "KSC_5601", "NEXT", "OCTETS", "SJIS_0208",
	"TIS620", "UNICODE_FSS", "UTF8", "WIN1250", "WIN1251",
	"WIN1252", "WIN1253", "WIN1254", "WIN1255", "WIN1256",
	"WIN1257", "WIN1258", "KOI8R", "KOI8U", "WIN_PTBR",
	"ISO8859_15", "GBK", "CP943C", "GB18030",
	NULL
};

/* {{{ _php_fbird_is_valid_charset
   Case-insensitive check of charset against the Firebird allowlist. */
static int _php_fbird_is_valid_charset(const char *charset)
{
	for (const char **cs = valid_charsets; *cs != NULL; cs++) {
		if (zend_binary_strcasecmp(charset, strlen(charset), *cs, strlen(*cs)) == 0) {
			return 1;
		}
	}
	return 0;
}
/* }}} */

/* {{{ proto resource fbird_create_database(string $database [, string $username [, string $password [, string $charset [, int $page_size]]]])
   Create a new Firebird database and return a connection resource */
PHP_FUNCTION(fbird_create_database)
{
	char *database = NULL, *username = NULL, *password = NULL, *charset = NULL;
	size_t database_len, username_len = 0, password_len = 0, charset_len = 0;
	zend_long page_size = 0;
	unsigned short dialect = 3;
	fbird_db_link *ib_link;
	char *create_sql = NULL;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|sssl",
			&database, &database_len,
			&username, &username_len,
			&password, &password_len,
			&charset, &charset_len,
			&page_size) == FAILURE) {
		return;
	}

	/* Validate charset against allowlist to prevent SQL injection via
	 * DEFAULT CHARACTER SET clause (unquoted identifier). */
	if (charset && charset_len > 0 && !_php_fbird_is_valid_charset(charset)) {
		php_error_docref(NULL, E_WARNING,
			"Invalid character set name '%s'", charset);
		RETURN_FALSE;
	}

	/* Escape single quotes in user-supplied strings to prevent SQL injection
	 * in the CREATE DATABASE statement (C1 fix, issue #155). */
	zend_string *esc_database = _php_fbird_escape_single_quotes(database, database_len);
	zend_string *esc_username = NULL;
	zend_string *esc_password = NULL;

	/* Build CREATE DATABASE SQL dynamically with spprintf() to eliminate
	 * fixed-size stack buffer overflow risk (M10 fix, issue #157). */
	spprintf(&create_sql, 0, "CREATE DATABASE '%s'", ZSTR_VAL(esc_database));

	if (username && username_len > 0) {
		char *tmp;
		esc_username = _php_fbird_escape_single_quotes(username, username_len);
		spprintf(&tmp, 0, "%s USER '%s'", create_sql, ZSTR_VAL(esc_username));
		efree(create_sql);
		create_sql = tmp;
	}
	if (password && password_len > 0) {
		char *tmp;
		esc_password = _php_fbird_escape_single_quotes(password, password_len);
		spprintf(&tmp, 0, "%s PASSWORD '%s'", create_sql, ZSTR_VAL(esc_password));
		efree(create_sql);
		create_sql = tmp;
	}
	if (page_size > 0) {
		char *tmp;
		spprintf(&tmp, 0, "%s PAGE_SIZE = %ld", create_sql, (long)page_size);
		efree(create_sql);
		create_sql = tmp;
	}
	if (charset && charset_len > 0) {
		char *tmp;
		/* charset is already validated against allowlist above */
		spprintf(&tmp, 0, "%s DEFAULT CHARACTER SET %s", create_sql, charset);
		efree(create_sql);
		create_sql = tmp;
	}

	void *create_result = fbc_create_database(
		IBG(master_instance),
		create_sql,
		dialect,
		IB_STATUS
	);

	/* Clean up dynamically allocated SQL and escaped strings */
	efree(create_sql);
	zend_string_release(esc_database);
	if (esc_username) {
		zend_string_release(esc_username);
	}
	if (esc_password) {
		zend_string_release(esc_password);
	}

	if (!create_result) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	ib_link = (fbird_db_link *) ecalloc(1, sizeof(fbird_db_link));
	ib_link->dialect = dialect;
	ib_link->tr_list = NULL;
	ib_link->event_head = NULL;
	ib_link->fbc_connection = create_result;

	/* Phase C: register resource and wrap in Firebird\Connection object.
	 * resource_list holds ref=1; set as default_link adds ref=2 (weak ref in object). */
	{
		zend_resource *cres = zend_register_resource(ib_link, le_link);
		if (IBG(default_link)) {
			zend_list_delete(IBG(default_link));
		}
		IBG(default_link) = cres;
		GC_ADDREF(cres); /* default_link ref */
		fbird_setup_connection_object(return_value, cres);
		/* no GC_DELREF: no extra caller ref was taken */
	}
}
/* }}} */

PHP_FUNCTION(fbird_drop_db)
{
	zval *link_arg = NULL;
	fbird_db_link *ib_link;
	fbird_tr_list *l;
	zend_resource *link_res;
	int drop_result;
	char *database = NULL, *username = NULL, *password = NULL;
	size_t database_len = 0, username_len = 0, password_len = 0;
	zend_bool string_mode = 0;

	RESET_ERRMSG;

	/* Try string overload first: fbird_drop_db(string $dsn, string $user, string $pass)
	 * Only activate if the first argument is actually a string (not resource/int/null) */
	if (ZEND_NUM_ARGS() >= 1) {
		zval *first_arg = NULL;
		zval tmp_args[1];
		if (zend_get_parameters_array_ex(1, tmp_args) == SUCCESS) {
			first_arg = &tmp_args[0];
			ZVAL_DEREF(first_arg);
		}
		if (first_arg && Z_TYPE_P(first_arg) == IS_STRING) {
			if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "s|ss",
					&database, &database_len, &username, &username_len, &password, &password_len) == SUCCESS
					&& database != NULL) {
				string_mode = 1;
			}
		}
	}

	if (string_mode) {
		/* Connect, drop, return */
		const char *u = username ? username : "SYSDBA";
		const char *p = password ? password : "masterkey";
		void *conn = fbc_connect(
			IBG(master_instance),
			database, database_len,
			u, strlen(u),
			p, strlen(p),
			NULL, 0,  /* charset */
			NULL, 0,  /* role */
			0,        /* num_buffers */
			3,        /* dialect */
			-1,       /* force_write */
			IB_STATUS
		);
		if (!conn) {
			_php_fbird_error();
			RETURN_FALSE;
		}
		drop_result = fbc_drop_database(conn, IB_STATUS);
		if (drop_result != 0) {
			_php_fbird_error();
			RETURN_FALSE;
		}
		RETURN_TRUE;
	}

	/* Original resource-based path: also accept Firebird\Connection object (Issue #120) */
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		return;
	}

	/* Type enforcement: resource only when explicit arg given (NULL = TypeError, no arg = default link) */
	if (ZEND_NUM_ARGS() >= 1) {
		if (link_arg == NULL) {
			zend_argument_type_error(1, "must be of type resource, null given");
			RETURN_THROWS();
		} else if (Z_TYPE_P(link_arg) != IS_RESOURCE && Z_TYPE_P(link_arg) != IS_OBJECT) {
			zend_argument_type_error(1, "must be of type resource, %s given", zend_get_type_by_const(Z_TYPE_P(link_arg)));
			RETURN_THROWS();
		}
	}

	if (ZEND_NUM_ARGS() == 0) {
		link_res = IBG(default_link);
		CHECK_LINK(link_res);
		IBG(default_link) = NULL;
	} else {
		link_res = _php_fbird_res_from_zval(link_arg);
		if (link_res == NULL) {
			RETURN_FALSE;
		}
	}

	ib_link = (fbird_db_link *)zend_fetch_resource2(link_res, LE_LINK, le_link, le_plink);

	if (!ib_link) {
		RETURN_FALSE;
	}

	/* OO API Only: All connections use fbc_drop_database() */
	if (ib_link->fbc_connection != NULL) {
		FBDEBUG("Dropping database via OO API...");
		drop_result = fbc_drop_database(ib_link->fbc_connection, IB_STATUS);
		if (drop_result != 0) {
			_php_fbird_error();
			RETURN_FALSE;
		}
		/* fbc_drop_database() already frees the connection wrapper */
		ib_link->fbc_connection = NULL;
	}

	/* drop_database() doesn't invalidate the transaction handles */
	for (l = ib_link->tr_list; l != NULL; l = l->next) {
		if (l->trans != NULL) {
			l->trans->fbt_transaction = NULL;
		}
	}

	zend_list_delete(link_res);

	RETURN_TRUE;
}

#endif /* HAVE_FIREBIRD */

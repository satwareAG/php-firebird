/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/**
 * Query preparation and cleanup functions.
 *
 * This file contains functions for preparing SQL statements, managing XSQLDA
 * structures, and cleaning up query resources.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"

#if HAVE_FIREBIRD

#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_query_internal.h"
#include "php_fbird_query_prepare.h"
#include "php_fbird_query_array.h"
#include "firebird_utils.h"

/* Exported for use in fbird_result.c and other files */
int le_query;

/* Implementation of _php_fbird_set_query_info */
int _php_fbird_set_query_info(fbird_query *ib_query)
{
	/*
	 * Firebird 3.0+ OO API - uses IStatement interface methods
	 *
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */

	/* Get statement type via OO API */
	ib_query->statement_type = fbs_get_type(IBG(master_instance), ib_query->fbs_statement, IB_STATUS);
	if (IB_STATUS[0] == 1 && IB_STATUS[1] != 0) {
		_php_fbird_error();
		return FAILURE;
	}

	/* Get field counts via OO API helper functions */
	ib_query->out_fields_count = fbs_get_output_count(IBG(master_instance), ib_query->fbs_statement, IB_STATUS);
	ib_query->in_fields_count = fbs_get_input_count(IBG(master_instance), ib_query->fbs_statement, IB_STATUS);

	return SUCCESS;
}

void _php_fbird_alloc_xsqlda_vars(XSQLDA *sqlda, ISC_SHORT *nullinds)
{
	int i;
	XSQLVAR *var;

	if (sqlda) {
		var = sqlda->sqlvar;
		for (i = 0; i < sqlda->sqld; i++, var++) {
			var->sqlind = &nullinds[i];

            /* Allocate sqldata buffer based on sqltype and sqllen */
            size_t code_size = 0;
            switch (var->sqltype & ~1) {
                case SQL_VARYING:
                    code_size = var->sqllen + sizeof(short);
                    break;
                case SQL_TEXT:
                    code_size = var->sqllen;
                    break;
                case SQL_ARRAY:
                case SQL_BLOB:
                    code_size = sizeof(ISC_QUAD);
                    break;
                default:
                    /* For fixed-size types (INTEGER, FLOAT, DATE, TIMESTAMP, BOOLEAN, etc.),
                       sqllen is reliable size. */
                    code_size = var->sqllen;
                    break;
            }

            if (code_size > 0) {
                /* Use ecalloc to zero-initialize the buffer to prevent garbage data */
                var->sqldata = ecalloc(1, code_size);
            } else {
                var->sqldata = NULL;
            }
		}
	}
}

void _php_fbird_free_xsqlda(XSQLDA *sqlda)
{
	int i;
	XSQLVAR *var;

	FBDEBUG("Free XSQLDA?");
	if (sqlda) {
		FBDEBUG("Freeing XSQLDA...");
		var = sqlda->sqlvar;
		for (i = 0; i < sqlda->sqld; i++, var++) {
			/* Only free if sqldata was allocated (may be NULL for OO API metadata-only XSQLDA) */
			if (var->sqldata) {
				efree(var->sqldata);
			}
		}
		efree(sqlda);
	}
}

void _php_fbird_free_query(fbird_query *ib_query)
{
	FBDEBUG("Freeing query...");

	if(ib_query->in_nullind)efree(ib_query->in_nullind);
	if(ib_query->out_nullind)efree(ib_query->out_nullind);
	if(ib_query->bind_buf)efree(ib_query->bind_buf);
	if(ib_query->in_sqlda)efree(ib_query->in_sqlda); // Note to myself: no need for _php_fbird_free_xsqlda()
	if(ib_query->out_sqlda)_php_fbird_free_xsqlda(ib_query->out_sqlda);
	if(ib_query->in_array)efree(ib_query->in_array);
	if(ib_query->out_array)efree(ib_query->out_array);
	if(ib_query->query)efree(ib_query->query);
	if(ib_query->ht_aliases)zend_array_destroy(ib_query->ht_aliases);
	if(ib_query->ht_ind)zend_array_destroy(ib_query->ht_ind);

	/* OO API message buffers (Phase 12+)
	 * Note: Metadata objects are released by fbs_free() when statement is freed,
	 * but we need to free the message buffers we allocated with safe_emalloc. */
	if (ib_query->out_msg_buffer) efree(ib_query->out_msg_buffer);
	if (ib_query->in_msg_buffer) efree(ib_query->in_msg_buffer);
	/* Metadata references are released when statement is freed - no efree needed here */

	efree(ib_query);
}

void php_fbird_free_query_rsrc(zend_resource *rsrc)
{
    fbird_query *ib_query = (fbird_query *)rsrc->ptr;

    if (ib_query != NULL) {
        /* Issue #294: Track whether this resource had an open cursor (SELECT result).
         * Only SELECT results should trigger default-transaction commit on free.
         * Prepared statements (fbird_prepare) that were never executed have no
         * open cursor and must NOT commit the default transaction. */
        bool had_open_cursor = (ib_query->fbs_resultset != NULL || ib_query->is_open);

        FBDEBUG("Preparing to free query by dtor...");

        /* If this is a child result, unlink it from the parent's list to prevent
         * use-after-free if the parent is subsequently freed.
         * Note: If we are being freed BY the parent (in the loop below), parent will
         * have already set ib_query->parent = NULL, so this block won't run. */
        if (ib_query->parent) {
            fbird_query **curr = &ib_query->parent->child_head;
            while (*curr) {
                if (*curr == ib_query) {
                    *curr = ib_query->child_next;
                    break;
                }
                curr = &(*curr)->child_next;
            }
            /* Do NOT call zend_list_free on parent - the parent query resource
             * should remain valid for subsequent fbird_execute() calls.
             * The parent's lifetime is controlled by the user, not by child results. */
        }

        /* Invalidate and free any dependent child result resources first so that
         * further use of those results triggers a TypeError as expected by tests. */
        fbird_query *child = ib_query->child_head;
        while (child) {
            fbird_query *next = child->child_next;
            /* Break the back-link to avoid cascading frees */
            child->parent = NULL;
            if (child->res) {
                /* Close child resource which marks it invalid for Zend */
                zend_list_close(child->res);
                child->res = NULL;
            }
            child = next;
        }
        /* Ensure any open cursor/statement is properly closed on the server
         * to avoid -502 (Attempt to reopen an open cursor) on subsequent uses.
         *
         * OO API Only: All statements use fbs_statement (no legacy stmt.stmt fallback)
         */
        if (ib_query->fbs_statement) {
            /* Close any open cursor first */
            if (ib_query->fbs_resultset || ib_query->is_open) {
                FBDEBUG("Closing open cursor in dtor (OO API)");
                fbs_close_cursor(ib_query->fbs_statement, IB_STATUS);
                ib_query->fbs_resultset = NULL;
                ib_query->is_open = 0;
                ib_query->has_more_rows = 0;
                /* If this is a child result that reused the parent's statement handle,
                 * mirror the cursor state reset to the parent to avoid double-close
                 * warnings on the next fbird_execute(). */
                if (ib_query->parent) {
                    ib_query->parent->is_open = 0;
                    ib_query->parent->has_more_rows = 0;
                }
            }
            /* Free the OO API statement only if this resource OWNS it.
             * Result clones created for SELECT reuse parent's handle and must NOT drop it. */
            if (ib_query->owns_stmt_handle) {
                fbs_free(ib_query->fbs_statement, IB_STATUS);
            }
            ib_query->fbs_statement = NULL;
        }

        /* Issue #294: True autocommit — commit the default (implicit) transaction
         * when a SELECT result is freed. trans_res == NULL indicates the default
         * transaction was used (no explicit transaction resource provided).
         * had_open_cursor ensures this only fires for SELECT results, not for
         * prepared statements (fbird_prepare) that were never executed.
         * IBG(in_mshutdown) guard prevents SIGSEGV during MSHUTDOWN cleanup
         * (Issue #295 — _php_fbird_commit_link handles MSHUTDOWN separately). */
        if (had_open_cursor && !ib_query->trans_res &&
            ib_query->trans && ib_query->trans->fbt_transaction &&
            !IBG(in_mshutdown)) {
            /* Issue #294: Commit the default transaction so the next autocommit
             * query starts a fresh transaction with a current snapshot.
             *
             * Only fire for the DEFAULT transaction (first tr_list node).
             * fbird_execute_auto() creates a temp transaction not in tr_list —
             * freeing it here would cause use-after-free when execute_auto
             * later calls fbt_rollback on the same pointer.
             *
             * Skip for persistent connections: cleanup_db() may drop the DB
             * during shutdown. The default tx is cleaned up by
             * _php_fbird_commit_link during MSHUTDOWN (with #295 guard).
             */
            bool is_default_tx = (ib_query->link && ib_query->link->tr_list &&
                ib_query->link->tr_list->trans == ib_query->trans);
            bool is_persistent = (ib_query->link && ib_query->link->is_persistent);
            if (is_default_tx && !is_persistent) {
                /* jane: silent on failure — see fbird_query_exec.c for rationale */
                fbt_commit(ib_query->trans->fbt_transaction, IB_STATUS);
                fbt_free(ib_query->trans->fbt_transaction);
                ib_query->trans->fbt_transaction = NULL;
            }
        }

        _php_fbird_free_query(ib_query);
    }
}

void php_fbird_query_minit(INIT_FUNC_ARGS)
{
	(void)type;
	le_query = zend_register_list_destructors_ex(php_fbird_free_query_rsrc, NULL,
		LE_QUERY, module_number);
}

int _php_fbird_prepare(fbird_query **new_query, fbird_db_link *link,
    fbird_transaction *trans, zend_resource *trans_res, char *query)
{
	/* Validate required parameters to prevent NULL pointer dereference */
	if (!link) {
		php_error_docref(NULL, E_WARNING, "Invalid database connection resource");
		return FAILURE;
	}
	if (!trans) {
		php_error_docref(NULL, E_WARNING, "Invalid transaction resource");
		return FAILURE;
	}
	if (!query) {
		php_error_docref(NULL, E_WARNING, "Query string is NULL");
		return FAILURE;
	}
	/* Return FAILURE, if querystring is empty */
	if (*query == '\0') {
		php_error_docref(NULL, E_WARNING, "Querystring empty.");
		return FAILURE;
	}

	fbird_query *ib_query = ecalloc(1, sizeof(fbird_query));
	/* Ensure linkage fields are initialized explicitly for clarity */
	ib_query->parent = NULL;
	ib_query->child_head = NULL;
	ib_query->child_next = NULL;
	/* Phase 5: Initialize OO API statement wrapper fields to NULL */
	ib_query->fbs_statement = NULL;
	ib_query->fbs_resultset = NULL;

	ib_query->res = zend_register_resource(ib_query, le_query);
	ib_query->link = link;
	ib_query->trans = trans;
	ib_query->trans_res = trans_res;
	ib_query->dialect = link->dialect;
	ib_query->query = estrdup(query);
	/* This prepared query owns the statement handle and is responsible for
	 * dropping it in the resource destructor. */
	ib_query->owns_stmt_handle = 1;

	/*
	 * Firebird 3.0+ OO API Statement Preparation
	 *
	 * Uses IStatement interface via fbs_prepare() wrapper.
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */
	void *attachment_ptr = fbc_get_attachment(link->fbc_connection);
	void *transaction_ptr = fbt_get_handle(trans->fbt_transaction);

	if (!attachment_ptr || !transaction_ptr) {
		_php_fbird_module_error("OO API connection/transaction pointers are NULL");
		goto _php_fbird_alloc_query_error;
	}

	ib_query->fbs_statement = fbs_prepare(
		IBG(master_instance),
		attachment_ptr,
		transaction_ptr,
		query,
		0,  /* sql_length: 0 = null-terminated */
		link->dialect,
		IB_STATUS
	);
	if (!ib_query->fbs_statement) {
		FBDEBUG("fbs_prepare() failed\n");
		_php_fbird_error();
		goto _php_fbird_alloc_query_error;
	}
	FBDEBUG("OO API statement prepared successfully\n");

	if(_php_fbird_set_query_info(ib_query)){
		goto _php_fbird_alloc_query_error;
	}

	/*
	 * OO API Message Buffer Allocation (Pure OO API - No Legacy)
	 *
	 * The OO API uses IMessageMetadata interfaces for metadata access
	 * and message buffers for data transfer during fetch operations.
	 * This is prepared for future migration of the fetch path.
	 */
	if (ib_query->out_fields_count > 0) {
		/* Get output metadata and allocate message buffer */
		ib_query->out_metadata = fbs_get_output_metadata(
			IBG(master_instance), ib_query->fbs_statement, IB_STATUS);
		if (!ib_query->out_metadata) {
			FBDEBUG("fbs_get_output_metadata() failed\n");
			_php_fbird_error();
			goto _php_fbird_alloc_query_error;
		}

		/* Get buffer size and allocate */
		ib_query->out_msg_length = fbm_get_message_length(
			IBG(master_instance), ib_query->out_metadata);
		if (ib_query->out_msg_length > 0) {
			ib_query->out_msg_buffer = safe_emalloc(1, ib_query->out_msg_length, 0);
			memset(ib_query->out_msg_buffer, 0, ib_query->out_msg_length);
		}
		FBDEBUG("OO API output message buffer allocated\n");

		/* Allocate out_sqlda from OO API metadata for compatibility with fbird_field_info()
		 * and for query clone logic in _php_fbird_exec(). */
		ib_query->out_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(ib_query->out_fields_count));
		ib_query->out_sqlda->version = SQLDA_VERSION1;
		ib_query->out_sqlda->sqln = ib_query->out_fields_count;
		ib_query->out_sqlda->sqld = ib_query->out_fields_count;

		/* Allocate null-indicator array used by result cloning.
		 * Legacy code expects ib_query->out_nullind to exist when out_sqlda exists.
		 * In OO-only mode, we still need the array even though actual NULL flags are
		 * read from out_msg_buffer via IMessageMetadata. */
		ib_query->out_nullind = safe_emalloc(sizeof(*ib_query->out_nullind),
			ib_query->out_fields_count, 0);
		memset(ib_query->out_nullind, 0, sizeof(*ib_query->out_nullind) * ib_query->out_fields_count);

		/* Populate each XSQLVAR from OO API output metadata */
		for (int i = 0; i < ib_query->out_fields_count; i++) {
			XSQLVAR *var = &ib_query->out_sqlda->sqlvar[i];
			const char *str_val;

			/* Get type and length from metadata */
			var->sqltype = fbm_get_type(IBG(master_instance), ib_query->out_metadata, i);
			var->sqllen = fbm_get_length(IBG(master_instance), ib_query->out_metadata, i);
			var->sqlscale = fbm_get_scale(IBG(master_instance), ib_query->out_metadata, i);
			var->sqlsubtype = fbm_get_subtype(IBG(master_instance), ib_query->out_metadata, i);

			/* Get field name */
			str_val = fbm_get_field(IBG(master_instance), ib_query->out_metadata, i);
			if (str_val && *str_val) {
				strncpy(var->sqlname, str_val, sizeof(var->sqlname) - 1);
				var->sqlname[sizeof(var->sqlname) - 1] = '\0';
				var->sqlname_length = (short)strlen(var->sqlname);
			} else {
				var->sqlname[0] = '\0';
				var->sqlname_length = 0;
			}

			/* Get relation (table) name */
			str_val = fbm_get_relation(IBG(master_instance), ib_query->out_metadata, i);
			if (str_val && *str_val) {
				strncpy(var->relname, str_val, sizeof(var->relname) - 1);
				var->relname[sizeof(var->relname) - 1] = '\0';
				var->relname_length = (short)strlen(var->relname);
			} else {
				var->relname[0] = '\0';
				var->relname_length = 0;
			}

			/* Owner name not available in OO API metadata - leave empty */
			var->ownname[0] = '\0';
			var->ownname_length = 0;

			/* Get alias name */
			str_val = fbm_get_alias(IBG(master_instance), ib_query->out_metadata, i);
			if (str_val && *str_val) {
				strncpy(var->aliasname, str_val, sizeof(var->aliasname) - 1);
				var->aliasname[sizeof(var->aliasname) - 1] = '\0';
				var->aliasname_length = (short)strlen(var->aliasname);
			} else {
				var->aliasname[0] = '\0';
				var->aliasname_length = 0;
			}

			/* Initialize data pointers to NULL (will be set during fetch) */
			var->sqldata = NULL;
			var->sqlind = NULL;
		}
		FBDEBUG("OO API out_sqlda populated from metadata\n");
	}

	if (ib_query->in_fields_count > 0) {
		/* Get input metadata and allocate message buffer */
		ib_query->in_metadata = fbs_get_input_metadata(
			IBG(master_instance), ib_query->fbs_statement, IB_STATUS);
		if (!ib_query->in_metadata) {
			FBDEBUG("fbs_get_input_metadata() failed\n");
			_php_fbird_error();
			goto _php_fbird_alloc_query_error;
		}

		/* Get buffer size and allocate */
		ib_query->in_msg_length = fbm_get_message_length(
			IBG(master_instance), ib_query->in_metadata);
		if (ib_query->in_msg_length > 0) {
			ib_query->in_msg_buffer = safe_emalloc(1, ib_query->in_msg_length, 0);
			memset(ib_query->in_msg_buffer, 0, ib_query->in_msg_length);
		}

		/* Also allocate bind_buf for parameter binding */
		ib_query->bind_buf = safe_emalloc(sizeof(BIND_BUF), ib_query->in_fields_count, 0);
		ib_query->in_nullind = safe_emalloc(sizeof(*ib_query->in_nullind), ib_query->in_fields_count, 0);

		/* Allocate in_sqlda from OO API metadata for compatibility with _php_fbird_bind().
		 * The binding logic still uses XSQLDA structures internally, so we need to
		 * populate in_sqlda with metadata from the OO API. */
		ib_query->in_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(ib_query->in_fields_count));
		ib_query->in_sqlda->version = SQLDA_VERSION1;
		ib_query->in_sqlda->sqln = ib_query->in_fields_count;
		ib_query->in_sqlda->sqld = ib_query->in_fields_count;

		/* Populate each XSQLVAR from OO API metadata */
		for (int i = 0; i < ib_query->in_fields_count; i++) {
			XSQLVAR *var = &ib_query->in_sqlda->sqlvar[i];

			/* Get type and length from metadata */
			var->sqltype = fbm_get_type(IBG(master_instance), ib_query->in_metadata, i);
			var->sqllen = fbm_get_length(IBG(master_instance), ib_query->in_metadata, i);
			var->sqlscale = fbm_get_scale(IBG(master_instance), ib_query->in_metadata, i);
			var->sqlsubtype = fbm_get_subtype(IBG(master_instance), ib_query->in_metadata, i);

			/* sqldata and sqlind will be set by _php_fbird_bind() to point to bind_buf */
			var->sqldata = NULL;
			var->sqlind = NULL;

			/* Populate relation/field names for input parameters.
			 * Needed for array binding (descriptor lookup via system tables).
			 * For ordinary scalar params, these may be empty and are ignored. */
			const char *str_val;

			str_val = fbm_get_field(IBG(master_instance), ib_query->in_metadata, i);
			if (str_val && *str_val) {
				strncpy(var->sqlname, str_val, sizeof(var->sqlname) - 1);
				var->sqlname[sizeof(var->sqlname) - 1] = '\0';
				var->sqlname_length = (short)strlen(var->sqlname);
			} else {
				var->sqlname[0] = '\0';
				var->sqlname_length = 0;
			}

			str_val = fbm_get_relation(IBG(master_instance), ib_query->in_metadata, i);
			if (str_val && *str_val) {
				strncpy(var->relname, str_val, sizeof(var->relname) - 1);
				var->relname[sizeof(var->relname) - 1] = '\0';
				var->relname_length = (short)strlen(var->relname);
			} else {
				var->relname[0] = '\0';
				var->relname_length = 0;
			}

			/* Owner/alias names not needed for input parameters */
			var->ownname[0] = '\0';
			var->ownname_length = 0;
			var->aliasname[0] = '\0';
			var->aliasname_length = 0;
		}

		FBDEBUG("OO API input SQLDA and message buffer allocated\n");
	}

	*new_query = ib_query;

	return SUCCESS;

_php_fbird_alloc_query_error:
	zend_list_delete(ib_query->res);

	return FAILURE;
}

#endif /* HAVE_FIREBIRD */

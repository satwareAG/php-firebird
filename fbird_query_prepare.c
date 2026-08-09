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
int _php_fbird_set_query_info(fbird_query *fb_query)
{
	ISC_STATUS status[256];
	/*
	 * Firebird 3.0+ OO API - uses IStatement interface methods
	 *
	 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
	 */

	/* Get statement type via OO API */
	fb_query->statement_type = fbs_get_type(FBG(master_instance), fb_query->fbs_statement, status);
	if (FB_STATUS_ERROR(status)) {
		_php_fbird_error(status);
		return FAILURE;
	}

	/* Get field counts via OO API helper functions */
	fb_query->out_fields_count = fbs_get_output_count(FBG(master_instance), fb_query->fbs_statement, status);
	fb_query->in_fields_count = fbs_get_input_count(FBG(master_instance), fb_query->fbs_statement, status);

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

void _php_fbird_free_query(fbird_query *fb_query)
{
	FBDEBUG("Freeing query...");

	if(fb_query->in_nullind)efree(fb_query->in_nullind);
	if(fb_query->out_nullind)efree(fb_query->out_nullind);
	if(fb_query->bind_buf)efree(fb_query->bind_buf);
	if(fb_query->in_sqlda)efree(fb_query->in_sqlda); // Note to myself: no need for _php_fbird_free_xsqlda()
	if(fb_query->out_sqlda)_php_fbird_free_xsqlda(fb_query->out_sqlda);
	if(fb_query->in_array)efree(fb_query->in_array);
	if(fb_query->out_array)efree(fb_query->out_array);
	if(fb_query->query)efree(fb_query->query);
	if(fb_query->ht_aliases)zend_array_destroy(fb_query->ht_aliases);
	if(fb_query->ht_ind)zend_array_destroy(fb_query->ht_ind);

	/* OO API message buffers (Phase 12+)
	 * Note: Metadata objects are released by fbs_free() when statement is freed,
	 * but we need to free the message buffers we allocated with safe_emalloc. */
	if (fb_query->out_msg_buffer) efree(fb_query->out_msg_buffer);
	if (fb_query->in_msg_buffer) efree(fb_query->in_msg_buffer);
	/* Metadata references are released when statement is freed - no efree needed here */

	efree(fb_query);
}

void php_fbird_free_query_rsrc(zend_resource *rsrc)
{
    fbird_query *fb_query = (fbird_query *)rsrc->ptr;
    ISC_STATUS status[256];

    if (fb_query != NULL) {
        /* Issue #294: Track whether this resource had an open cursor (SELECT result).
         * Only SELECT results should trigger default-transaction commit on free.
         * Prepared statements (fbird_prepare) that were never executed have no
         * open cursor and must NOT commit the default transaction. */
        bool had_open_cursor = (fb_query->fbs_resultset != NULL || fb_query->is_open);

        FBDEBUG("Preparing to free query by dtor...");

        /* If this is a child result, unlink it from the parent's list to prevent
         * use-after-free if the parent is subsequently freed.
         * Note: If we are being freed BY the parent (in the loop below), parent will
         * have already set fb_query->parent = NULL, so this block won't run. */
        if (fb_query->parent) {
            fbird_query **curr = &fb_query->parent->child_head;
            while (*curr) {
                if (*curr == fb_query) {
                    *curr = fb_query->child_next;
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
        fbird_query *child = fb_query->child_head;
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
        if (fb_query->fbs_statement) {
            /* Close any open cursor first */
            if (fb_query->fbs_resultset || fb_query->is_open) {
                FBDEBUG("Closing open cursor in dtor (OO API)");
                fbs_close_cursor(fb_query->fbs_statement, status);
                fb_query->fbs_resultset = NULL;
                _php_fbird_cursor_closed(fb_query);  /* Issue #566: decrement counter */
                fb_query->has_more_rows = 0;
                /* If this is a child result that reused the parent's statement handle,
                 * mirror the cursor state reset to the parent to avoid double-close
                 * warnings on the next fbird_execute(). */
                if (fb_query->parent) {
                    _php_fbird_cursor_closed(fb_query->parent);
                    fb_query->parent->has_more_rows = 0;
                }
            }
            /* Free the OO API statement only if this resource OWNS it.
             * Result clones created for SELECT reuse parent's handle and must NOT drop it. */
            if (fb_query->owns_stmt_handle) {
                fbs_free(fb_query->fbs_statement, status);
            }
            fb_query->fbs_statement = NULL;
        }

        /* Issue #294: True autocommit — commit the default (implicit) transaction
         * when a SELECT result is freed. trans_res == NULL indicates the default
         * transaction was used (no explicit transaction resource provided).
         * had_open_cursor ensures this only fires for SELECT results, not for
         * prepared statements (fbird_prepare) that were never executed.
         * FBG(in_mshutdown) guard prevents SIGSEGV during MSHUTDOWN cleanup
         * (Issue #295 — _php_fbird_commit_link handles MSHUTDOWN separately). */
        if (had_open_cursor && !fb_query->trans_res &&
            fb_query->trans && fb_query->trans->fbt_transaction &&
            !FBG(in_mshutdown)) {
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
            bool is_default_tx = (fb_query->link && fb_query->link->tr_list &&
                fb_query->link->tr_list->trans == fb_query->trans);
            bool is_persistent = (fb_query->link && fb_query->link->is_persistent);
            if (is_default_tx && !is_persistent) {
                /* jane: silent on failure — see fbird_query_exec.c for rationale */
                fbt_commit(fb_query->trans->fbt_transaction, status);
                fbt_free(fb_query->trans->fbt_transaction);
                fb_query->trans->fbt_transaction = NULL;
            }
        }

        _php_fbird_free_query(fb_query);
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
	ISC_STATUS status[256];
	/* Validate required parameters to prevent NULL pointer dereference */
	if (!link) {
		_php_fbird_module_error("Invalid database connection resource");
		return FAILURE;
	}
	if (!trans) {
		_php_fbird_module_error("Invalid transaction resource");
		return FAILURE;
	}
	if (!query) {
		_php_fbird_module_error("Query string is NULL");
		return FAILURE;
	}
	/* Return FAILURE, if querystring is empty */
	if (*query == '\0') {
		_php_fbird_module_error("Querystring empty.");
		return FAILURE;
	}

	fbird_query *fb_query = ecalloc(1, sizeof(fbird_query));
	/* Ensure linkage fields are initialized explicitly for clarity */
	fb_query->parent = NULL;
	fb_query->child_head = NULL;
	fb_query->child_next = NULL;
	/* Phase 5: Initialize OO API statement wrapper fields to NULL */
	fb_query->fbs_statement = NULL;
	fb_query->fbs_resultset = NULL;

	fb_query->res = zend_register_resource(fb_query, le_query);
	fb_query->link = link;
	fb_query->trans = trans;
	fb_query->trans_res = trans_res;
	fb_query->dialect = link->dialect;
	fb_query->query = estrdup(query);
	/* This prepared query owns the statement handle and is responsible for
	 * dropping it in the resource destructor. */
	fb_query->owns_stmt_handle = 1;

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

	fb_query->fbs_statement = fbs_prepare(
		FBG(master_instance),
		attachment_ptr,
		transaction_ptr,
		query,
		0,  /* sql_length: 0 = null-terminated */
		link->dialect,
		status
	);
	if (!fb_query->fbs_statement) {
		FBDEBUG("fbs_prepare() failed\n");
		_php_fbird_error(status);
		goto _php_fbird_alloc_query_error;
	}
	FBDEBUG("OO API statement prepared successfully\n");

	if(_php_fbird_set_query_info(fb_query)){
		goto _php_fbird_alloc_query_error;
	}

	/*
	 * OO API Message Buffer Allocation (Pure OO API - No Legacy)
	 *
	 * The OO API uses IMessageMetadata interfaces for metadata access
	 * and message buffers for data transfer during fetch operations.
	 * This is prepared for future migration of the fetch path.
	 */
	if (fb_query->out_fields_count > 0) {
		/* Get output metadata and allocate message buffer */
		fb_query->out_metadata = fbs_get_output_metadata(
			FBG(master_instance), fb_query->fbs_statement, status);
		if (!fb_query->out_metadata) {
			FBDEBUG("fbs_get_output_metadata() failed\n");
			_php_fbird_error(status);
			goto _php_fbird_alloc_query_error;
		}

		/* Get buffer size and allocate */
		fb_query->out_msg_length = fbm_get_message_length(
			FBG(master_instance), fb_query->out_metadata);
		if (fb_query->out_msg_length > 0) {
			fb_query->out_msg_buffer = safe_emalloc(1, fb_query->out_msg_length, 0);
			memset(fb_query->out_msg_buffer, 0, fb_query->out_msg_length);
		}
		FBDEBUG("OO API output message buffer allocated\n");

		/* Allocate out_sqlda from OO API metadata for compatibility with fbird_field_info()
		 * and for query clone logic in _php_fbird_exec(). */
		fb_query->out_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(fb_query->out_fields_count));
		fb_query->out_sqlda->version = SQLDA_VERSION1;
		fb_query->out_sqlda->sqln = fb_query->out_fields_count;
		fb_query->out_sqlda->sqld = fb_query->out_fields_count;

		/* Allocate null-indicator array used by result cloning.
		 * Legacy code expects fb_query->out_nullind to exist when out_sqlda exists.
		 * In OO-only mode, we still need the array even though actual NULL flags are
		 * read from out_msg_buffer via IMessageMetadata. */
		fb_query->out_nullind = safe_emalloc(sizeof(*fb_query->out_nullind),
			fb_query->out_fields_count, 0);
		memset(fb_query->out_nullind, 0, sizeof(*fb_query->out_nullind) * fb_query->out_fields_count);

		/* Populate each XSQLVAR from OO API output metadata */
		for (int i = 0; i < fb_query->out_fields_count; i++) {
			XSQLVAR *var = &fb_query->out_sqlda->sqlvar[i];
			const char *str_val;

			/* Get type and length from metadata */
			var->sqltype = fbm_get_type(FBG(master_instance), fb_query->out_metadata, i);
			var->sqllen = fbm_get_length(FBG(master_instance), fb_query->out_metadata, i);
			var->sqlscale = fbm_get_scale(FBG(master_instance), fb_query->out_metadata, i);
			var->sqlsubtype = fbm_get_subtype(FBG(master_instance), fb_query->out_metadata, i);

			/* Get field name */
			str_val = fbm_get_field(FBG(master_instance), fb_query->out_metadata, i);
			if (str_val && *str_val) {
				strncpy(var->sqlname, str_val, sizeof(var->sqlname) - 1);
				var->sqlname[sizeof(var->sqlname) - 1] = '\0';
				var->sqlname_length = (short)strlen(var->sqlname);
			} else {
				var->sqlname[0] = '\0';
				var->sqlname_length = 0;
			}

			/* Get relation (table) name */
			str_val = fbm_get_relation(FBG(master_instance), fb_query->out_metadata, i);
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
			str_val = fbm_get_alias(FBG(master_instance), fb_query->out_metadata, i);
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

	if (fb_query->in_fields_count > 0) {
		/* Get input metadata and allocate message buffer */
		fb_query->in_metadata = fbs_get_input_metadata(
			FBG(master_instance), fb_query->fbs_statement, status);
		if (!fb_query->in_metadata) {
			FBDEBUG("fbs_get_input_metadata() failed\n");
			_php_fbird_error(status);
			goto _php_fbird_alloc_query_error;
		}

		/* Get buffer size and allocate */
		fb_query->in_msg_length = fbm_get_message_length(
			FBG(master_instance), fb_query->in_metadata);
		if (fb_query->in_msg_length > 0) {
			fb_query->in_msg_buffer = safe_emalloc(1, fb_query->in_msg_length, 0);
			memset(fb_query->in_msg_buffer, 0, fb_query->in_msg_length);
		}

		/* Also allocate bind_buf for parameter binding */
		fb_query->bind_buf = safe_emalloc(sizeof(BIND_BUF), fb_query->in_fields_count, 0);
		fb_query->in_nullind = safe_emalloc(sizeof(*fb_query->in_nullind), fb_query->in_fields_count, 0);

		/* Allocate in_sqlda from OO API metadata for compatibility with _php_fbird_bind().
		 * The binding logic still uses XSQLDA structures internally, so we need to
		 * populate in_sqlda with metadata from the OO API. */
		fb_query->in_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(fb_query->in_fields_count));
		fb_query->in_sqlda->version = SQLDA_VERSION1;
		fb_query->in_sqlda->sqln = fb_query->in_fields_count;
		fb_query->in_sqlda->sqld = fb_query->in_fields_count;

		/* Populate each XSQLVAR from OO API metadata */
		for (int i = 0; i < fb_query->in_fields_count; i++) {
			XSQLVAR *var = &fb_query->in_sqlda->sqlvar[i];

			/* Get type and length from metadata */
			var->sqltype = fbm_get_type(FBG(master_instance), fb_query->in_metadata, i);
			var->sqllen = fbm_get_length(FBG(master_instance), fb_query->in_metadata, i);
			var->sqlscale = fbm_get_scale(FBG(master_instance), fb_query->in_metadata, i);
			var->sqlsubtype = fbm_get_subtype(FBG(master_instance), fb_query->in_metadata, i);

			/* sqldata and sqlind will be set by _php_fbird_bind() to point to bind_buf */
			var->sqldata = NULL;
			var->sqlind = NULL;

			/* Populate relation/field names for input parameters.
			 * Needed for array binding (descriptor lookup via system tables).
			 * For ordinary scalar params, these may be empty and are ignored. */
			const char *str_val;

			str_val = fbm_get_field(FBG(master_instance), fb_query->in_metadata, i);
			if (str_val && *str_val) {
				strncpy(var->sqlname, str_val, sizeof(var->sqlname) - 1);
				var->sqlname[sizeof(var->sqlname) - 1] = '\0';
				var->sqlname_length = (short)strlen(var->sqlname);
			} else {
				var->sqlname[0] = '\0';
				var->sqlname_length = 0;
			}

			str_val = fbm_get_relation(FBG(master_instance), fb_query->in_metadata, i);
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

	*new_query = fb_query;

	return SUCCESS;

_php_fbird_alloc_query_error:
	zend_list_delete(fb_query->res);

	return FAILURE;
}

#endif /* HAVE_FIREBIRD */

/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

/* Enable GNU features (includes X/Open for strptime, BSD for strlcpy/tm_zone) */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <ctype.h>
#include <string.h>

#include "php.h"
#include "php_ini.h"

#if HAVE_FIREBIRD

#include "ext/standard/php_standard.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_query_internal.h"
#include "php_fbird_query_prepare.h"
#include "php_fbird_query_bind.h"
#include "php_fbird_query_array.h"
#include "firebird_utils.h"
#include "fbird_classes.h"
#include "src/php_fbird_compat.h"

/* le_query is defined in fbird_query_prepare.c */

/* Helper: fetch fbird_transaction* and optional zend_resource* from either a
 * le_trans resource zval or a Firebird\Transaction object zval.
 * Sets *res_out (if non-NULL) to the underlying zend_resource*.
 * Returns NULL if z does not hold a valid le_trans. */
static fbird_transaction *_php_fbird_trans_from_zval(zval *z, zend_resource **res_out)
{
    if (!z) return NULL;
    if (Z_TYPE_P(z) == IS_RESOURCE) {
        fbird_transaction *t = (fbird_transaction *)zend_fetch_resource_ex(z, NULL, le_trans);
        if (t && res_out) *res_out = Z_RES_P(z);
        return t;
    }
    /* Only treat as Transaction object if it actually IS a Firebird\Transaction.
     * Without this check, a Firebird\Connection object would be misinterpreted
     * causing wrong memory access via fbird_transaction_get_resource(). */
    if (Z_TYPE_P(z) == IS_OBJECT &&
            instanceof_function(Z_OBJCE_P(z), fbird_transaction_ce)) {
        zend_resource *res = fbird_transaction_get_resource(Z_OBJ_P(z));
        if (res) {
            fbird_transaction *t = (fbird_transaction *)zend_fetch_resource(res, "Firebird transaction", le_trans);
            if (t && res_out) *res_out = res;
            return t;
        }
    }
    return NULL;
}

/* Helper: fetch fbird_db_link* from either a le_link/le_plink resource zval
 * or a Firebird\Connection object zval.
 * Returns NULL if z does not hold a valid connection.
 * Uses instanceof_function guard to prevent misinterpreting Transaction objects. */
static fbird_db_link *_php_fbird_link_from_zval(zval *z)
{
    if (!z) return NULL;
    ZVAL_DEREF(z);
    if (Z_TYPE_P(z) == IS_RESOURCE) {
        fbird_db_link *link = (fbird_db_link *)zend_fetch_resource_ex(z, NULL, le_link);
        if (!link) link = (fbird_db_link *)zend_fetch_resource_ex(z, NULL, le_plink);
        return link;
    }
    /* Only treat as Connection object if it actually IS a Firebird\Connection.
     * Without this guard, a Firebird\Transaction object could be misinterpreted. */
    if (Z_TYPE_P(z) == IS_OBJECT &&
            instanceof_function(Z_OBJCE_P(z), fbird_connection_ce)) {
        zend_resource *conn_res = fbird_connection_get_resource(Z_OBJ_P(z));
        if (conn_res) {
            return (fbird_db_link *)zend_fetch_resource2(conn_res, LE_LINK, le_link, le_plink);
        }
    }
    return NULL;
}

int _php_fbird_exec(INTERNAL_FUNCTION_PARAMETERS, fbird_query *fb_query, zval *args, int bind_n)
{
	ISC_STATUS status[256];
	int rv = FAILURE;
	ISC_STATUS isc_result;
	int argc = fb_query->in_fields_count;

	(void)execute_data;

	RESET_ERRMSG;
	RETVAL_FALSE;

	/* Enhanced parameter validation BEFORE Firebird API calls.
	 *
	 * Validation behavior:
	 *   - bind_n < 0 or argc < 0: Invalid state → E_WARNING + FAILURE
	 *   - bind_n < argc (too few args given): E_WARNING + FAILURE (execution blocked)
	 *   - bind_n > argc (extra args given): E_NOTICE (execution continues, extra args ignored)
	 *
	 * This stricter validation provides clearer error messages compared to letting
	 * Firebird return cryptic errors for parameter mismatches.
	 *
	 * Test coverage: tests/bug45373.phpt
	 * Documentation: docs/development/FBIRD_QUERY_EXEC_FIXES.md
	 */
	if (bind_n < 0 || argc < 0) {
		_php_fbird_module_error("Invalid parameter count: bind_n=%d, argc=%d", bind_n, argc);
		return FAILURE;
	}

	if (bind_n != argc) {
		if (bind_n < argc) {
			/* Too few args: error, cannot proceed */
			_php_fbird_module_error(
				"Statement expects %d arguments, %d given", argc, bind_n);
			return FAILURE;
		} else {
			/* Extra args: notice, execution continues (backward compatible) */
			php_error_docref(NULL, E_NOTICE,
				"Statement expects %d arguments, %d given", argc, bind_n);
		}
	}

 /* Cursor lifecycle management: before any re-execution, close an open cursor
  * unconditionally to match legacy semantics and avoid -502 reopen errors. */
 if (fb_query->statement_type != isc_info_sql_stmt_exec_procedure && fb_query->is_open) {
     FBDEBUG("Closing open cursor before re-execution");
     /* Be tolerant: silently ignore ALL errors when attempting to close the cursor
      * before re-execution. The cursor may already have been closed by various means
      * (fbird_free_result, transaction commit, EOF reached, etc.) - this is expected
      * and should not generate warnings. We unconditionally reset the is_open flag. */

     /* OO API Only: Close cursor via fbs_close_cursor() */
     if (fb_query->fbs_statement) {
         fbs_close_cursor(fb_query->fbs_statement, status);
     }
     fb_query->is_open = 0;
     fb_query->has_more_rows = 0;
 }

	/* NOTE: We intentionally do NOT use SEPARATE_ZVAL here.
	 *
	 * Previously, SEPARATE_ZVAL was called on each argument to create independent
	 * copies before binding. However, this caused memory corruption issues where
	 * the PHP caller's original array variables would be corrupted after fbird_query
	 * returned.
	 *
	 * The root cause was that SEPARATE_ZVAL's interaction with PHP 8's copy-on-write
	 * semantics and the parameter passing mechanism corrupted the original zval's
	 * HashTable.
	 *
	 * The fix is to NOT separate the zvals. Instead, _php_fbird_bind_array() now
	 * uses zval_get_long(), zval_get_double(), zval_get_string() which create
	 * temporary copies of values without modifying the original zval in-place.
	 * This preserves the caller's arrays while still allowing Firebird binding.
	 */

	switch (fb_query->statement_type) {
		fbird_tr_list **l;
		fbird_transaction *trans;

		case isc_info_sql_stmt_start_trans:

			/* OO API path: Use fbt_start() for OO API connections.
			 * This avoids calling isc_dsql_execute_immediate() with invalid legacy handles.
			 * Note: SET TRANSACTION SQL parameters are not parsed here - uses default TPB. */
			if (fb_query->link && fb_query->link->fbc_connection) {
				void *attachment = fbc_get_attachment(fb_query->link->fbc_connection);
				void *new_trans = NULL;

				FBDEBUG("OO API: Executing SET TRANSACTION via fbt_start()");

				/* Start transaction with default TPB (READ_WRITE, WAIT, CONCURRENCY) */
				new_trans = fbt_start(FBG(master_instance), attachment, 0, NULL, status);
				if (!new_trans) {
					_php_fbird_error(status);
					goto _php_fbird_ex_error;
				}

				trans = (fbird_transaction *) emalloc(sizeof(fbird_transaction));
				trans->link_cnt = 1;
				trans->affected_rows = 0;
				trans->fbt_transaction = new_trans;
				trans->db_link[0] = fb_query->link;

				if (fb_query->link->tr_list == NULL) {
					fb_query->link->tr_list = (fbird_tr_list *) emalloc(sizeof(fbird_tr_list));
					fb_query->link->tr_list->trans = NULL;
					fb_query->link->tr_list->next = NULL;
				}

				/* link the transaction into the connection-transaction list */
				for (l = &fb_query->link->tr_list; *l != NULL; l = &(*l)->next);
				*l = (fbird_tr_list *) emalloc(sizeof(fbird_tr_list));
				(*l)->trans = trans;
				(*l)->next = NULL;

				RETVAL_RES(zend_register_resource(trans, le_trans));
				Z_TRY_ADDREF_P(return_value);

				return SUCCESS;
			}

			/* OO API Only: Connection must have OO API handle for SET TRANSACTION */
			_php_fbird_module_error("SET TRANSACTION requires OO API connection (fbc_connection required)");
			goto _php_fbird_ex_error;

		case isc_info_sql_stmt_commit:
		case isc_info_sql_stmt_rollback:

			/* OO API path: Use fbt_commit()/fbt_rollback() for OO API transactions.
			 * This avoids calling isc_dsql_execute_immediate() with invalid legacy handles.
			 *
			 * IMPORTANT: fbt_commit/fbt_rollback return 0 on success, non-zero on error.
			 */
			if (fb_query->trans && fb_query->trans->fbt_transaction) {
				int rc;
				if (fb_query->statement_type == isc_info_sql_stmt_commit) {
					FBDEBUG("OO API: Executing COMMIT via fbt_commit()");
					rc = fbt_commit(fb_query->trans->fbt_transaction, status);
				} else {
					FBDEBUG("OO API: Executing ROLLBACK via fbt_rollback()");
					rc = fbt_rollback(fb_query->trans->fbt_transaction, status);
				}
				if (rc != 0) {
					_php_fbird_error(status);
					goto _php_fbird_ex_error;
				}

				/* Mark transaction as closed.
				 * Note: Do NOT delete the PHP resource here.
				 * Legacy behavior keeps the resource alive but invalid for further use.
				 */
				fb_query->trans->fbt_transaction = NULL;

				RETVAL_TRUE;
				return SUCCESS;
			}

			/* OO API Only: Transaction must have OO API handle for COMMIT/ROLLBACK */
			_php_fbird_module_error("COMMIT/ROLLBACK requires OO API transaction (fbt_transaction required)");
			goto _php_fbird_ex_error;

		default:
			RETVAL_FALSE;
	}

	if (fb_query->in_fields_count) { /* has placeholders */
		FBDEBUG("Query wants XSQLDA for input");
		if (_php_fbird_bind(fb_query, args) == FAILURE) {
			FBDEBUG("Could not bind input XSQLDA");
			goto _php_fbird_ex_error;
		}

		/* Verify OO API message buffer infrastructure is available.
		 * If in_metadata or in_msg_buffer is not set, OO API execution
		 * with parameters is not possible - report clear error. */
		if (!fb_query->in_metadata) {
			_php_fbird_module_error("OO API input metadata not available for parameterized query");
			goto _php_fbird_ex_error;
		}
		if (!fb_query->in_msg_buffer) {
			_php_fbird_module_error("OO API input message buffer not allocated for parameterized query");
			goto _php_fbird_ex_error;
		}

		/* Transfer bound XSQLDA values to OO API message buffer.
		 * This is required because the OO API uses flat message buffers
		 * with offsets from IMessageMetadata, not XSQLDA structures. */
		if (_php_fbird_xsqlda_to_msg_buffer(fb_query) == FAILURE) {
			FBDEBUG("Could not transfer XSQLDA to message buffer");
			goto _php_fbird_ex_error;
		}
	}

    isc_result = 0;

    /* Issue #294: If the default transaction was committed by autocommit
     * (fbt_transaction is NULL), restart it before executing. This allows
     * fbird_execute() on prepared statements created with the default tx
     * to work after a fbird_query() DML call committed the default tx.
     * trans_res == NULL ensures we only restart the default (implicit) tx,
     * not an explicit user-started transaction. */
    if (fb_query->trans && fb_query->trans->fbt_transaction == NULL &&
        fb_query->trans_res == NULL &&
        fb_query->link && fb_query->link->fbc_connection) {
        fb_query->trans = NULL;  /* force _php_fbird_def_trans to restart */
        if (SUCCESS != _php_fbird_def_trans(fb_query->link, &fb_query->trans)) {
            return FAILURE;  /* _php_fbird_def_trans already reported the error */
        }
    }

    if (fb_query->fbs_statement && fb_query->trans && fb_query->trans->fbt_transaction) {
        void *transaction_ptr = fbt_get_handle(fb_query->trans->fbt_transaction);
        int oo_api_success = 0;

        if (transaction_ptr) {
            /* For SELECT statements, open cursor using OO API */
            if (fb_query->statement_type == isc_info_sql_stmt_select ||
                fb_query->statement_type == isc_info_sql_stmt_select_for_upd) {
                /*
                 * OO API cursor open for SELECT.
                 * Uses IMessageMetadata for parameter binding (Firebird 3.0+ OO API).
                 * Input parameters are passed via in_msg_buffer/in_metadata.
                 */
                oo_api_success = fbs_open_cursor(
                    FBG(master_instance),
                    fb_query->fbs_statement,
                    transaction_ptr,
                    fb_query->in_msg_buffer,  /* in_msg: parameter values */
                    fb_query->in_metadata,    /* in_metadata: parameter metadata */
                    0,    /* cursor_flags: default */
                    status
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_open_cursor() succeeded for SELECT");
                    isc_result = 0; /* Success */
                } else {
                    /* OO API cursor open failed - report error immediately, no fallback */
                    _php_fbird_error(status);
                    goto _php_fbird_ex_error;
                }
            }
            /* For non-SELECT (INSERT/UPDATE/DELETE) without RETURNING, use fbs_execute */
            else if ((fb_query->statement_type == isc_info_sql_stmt_insert ||
                      fb_query->statement_type == isc_info_sql_stmt_update ||
                      fb_query->statement_type == isc_info_sql_stmt_delete) &&
                     !fb_query->out_sqlda) {
                /*
                 * OO API execute for DML (no RETURNING).
                 * Uses IMessageMetadata for parameter binding (Firebird 3.0+ OO API).
                 * Input parameters are passed via in_msg_buffer/in_metadata.
                 */
                oo_api_success = fbs_execute(
                    FBG(master_instance),
                    fb_query->fbs_statement,
                    transaction_ptr,
                    fb_query->in_msg_buffer,  /* in_msg: parameter values */
                    fb_query->in_metadata,    /* in_metadata: parameter metadata */
                    NULL, /* out_msg: no output for DML without RETURNING */
                    NULL, /* out_metadata: no output for DML without RETURNING */
                    status
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_execute() succeeded for DML");
                    isc_result = 0; /* Success */
                } else {
                    /* OO API execution failed - report error immediately, no fallback */
                    _php_fbird_error(status);
                    goto _php_fbird_ex_error;
                }
            }
            /* For DDL statements (CREATE, DROP, ALTER, etc.), use fbs_execute */
            else if (fb_query->statement_type == isc_info_sql_stmt_ddl) {
                /*
                 * DDL statements have no input/output parameters.
                 * Execute directly via OO API.
                 */
                oo_api_success = fbs_execute(
                    FBG(master_instance),
                    fb_query->fbs_statement,
                    transaction_ptr,
                    NULL, /* in_msg */
                    NULL, /* in_metadata */
                    NULL, /* out_msg */
                    NULL, /* out_metadata */
                    status
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_execute() succeeded for DDL");
                    isc_result = 0; /* Success */
                } else {
                    /* OO API execution failed - report error immediately, no fallback */
                    _php_fbird_error(status);
                    goto _php_fbird_ex_error;
                }
            }
            /* SAVEPOINT statements (SAVEPOINT / ROLLBACK TO SAVEPOINT / RELEASE SAVEPOINT)
             * are reported as statement type 14. They have no input/output parameters.
             */
            else if (fb_query->statement_type == isc_info_sql_stmt_savepoint) {
                oo_api_success = fbs_execute(
                    FBG(master_instance),
                    fb_query->fbs_statement,
                    transaction_ptr,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    status
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_execute() succeeded for SAVEPOINT");
                    isc_result = 0;
                } else {
                    _php_fbird_error(status);
                    goto _php_fbird_ex_error;
                }
            }
            /* For EXECUTE PROCEDURE - use fbs_execute with input and output buffers */
            else if (fb_query->statement_type == isc_info_sql_stmt_exec_procedure) {
                /*
                 * OO API execute for EXECUTE PROCEDURE.
                 * Uses IMessageMetadata for parameter binding (Firebird 3.0+ OO API).
                 * Input parameters are passed via in_msg_buffer/in_metadata.
                 * Output parameters are returned via out_msg_buffer/out_metadata.
                 */
                oo_api_success = fbs_execute(
                    FBG(master_instance),
                    fb_query->fbs_statement,
                    transaction_ptr,
                    fb_query->in_msg_buffer,   /* in_msg: input parameter values */
                    fb_query->in_metadata,     /* in_metadata: input parameter metadata */
                    fb_query->out_msg_buffer,  /* out_msg: output parameter values */
                    fb_query->out_metadata,    /* out_metadata: output parameter metadata */
                    status
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_execute() succeeded for EXECUTE PROCEDURE");
                    isc_result = 0; /* Success */
                } else {
                    /* OO API execution failed - report error immediately, no fallback */
                    _php_fbird_error(status);
                    goto _php_fbird_ex_error;
                }
            }
            /* For DML with RETURNING - open cursor, fetch 1 row into out_msg_buffer, close cursor */
            else if ((fb_query->statement_type == isc_info_sql_stmt_insert ||
                      fb_query->statement_type == isc_info_sql_stmt_update ||
                      fb_query->statement_type == isc_info_sql_stmt_delete) &&
                     fb_query->out_sqlda) {
                /*
                 * OO API handling for DML with RETURNING clause.
                 *
                 * This behaves like legacy isc_dsql_execute2():
                 * - execute statement
                 * - copy one result row into out_msg_buffer
                 * - close cursor immediately
                 *
                 * RETURNING typically yields exactly one row.
                 */
                oo_api_success = fbs_open_cursor(
                    FBG(master_instance),
                    fb_query->fbs_statement,
                    transaction_ptr,
                    fb_query->in_msg_buffer,
                    fb_query->in_metadata,
                    0,
                    status
                );

                if (!oo_api_success) {
                    _php_fbird_error(status);
                    goto _php_fbird_ex_error;
                }

                int fetch_result = fbs_fetch(
                    FBG(master_instance),
                    fb_query->fbs_statement,
                    fb_query->out_msg_buffer,
                    status
                );

                if (fetch_result == -1) {
                    _php_fbird_error(status);
                    fbs_close_cursor(fb_query->fbs_statement, status);
                    goto _php_fbird_ex_error;
                }

                /* Always close cursor for DML RETURNING (like execute2) */
                fbs_close_cursor(fb_query->fbs_statement, status);

                /* fetch_result: 1=row copied into out_msg_buffer, 0=no data */
                if (fetch_result == 1) {
                    fb_query->was_result_once = 1;
                }
                isc_result = 0;
            }
            /* Unhandled statement type for OO API */
            else {
                _php_fbird_module_error("Statement type %d not supported via OO API",
                    fb_query->statement_type);
                goto _php_fbird_ex_error;
            }
        }

        /* If OO API succeeded, skip to done */
        if (oo_api_success) {
            goto execute_done;
        }
        /* Should not reach here - all paths either succeed or error out above */
        _php_fbird_module_error("OO API execution path did not complete");
        goto _php_fbird_ex_error;
    }

    /*
     * Non-OO API path: This connection does not have OO API transaction.
     * This should only happen for legacy connections (without fbt_transaction).
     * For now, report an error as we're removing legacy API support.
     */
    _php_fbird_module_error("Legacy API execution not supported. Connection must use OO API (fbt_transaction required)");
    goto _php_fbird_ex_error;

execute_done:

    if (isc_result) {
        FBDEBUG("Could not execute query");
        _php_fbird_error(status);
        goto _php_fbird_ex_error;
    }

    fb_query->trans->affected_rows = 0;

    /* For SELECT statements, mark cursor state as open with rows pending.
     * Check both legacy (out_sqlda) and OO API (fbs_statement) paths. */
    if ((fb_query->statement_type == isc_info_sql_stmt_select ||
         fb_query->statement_type == isc_info_sql_stmt_select_for_upd) &&
        (fb_query->out_sqlda || fb_query->fbs_statement)) {
        fb_query->is_open = 1;
        fb_query->has_more_rows = 1;

        /* OO API SELECT path: return the query resource directly when no SQLDA.
         * The cursor is open via fbs_open_cursor() and fetching will use fbs_fetch(). */
        if (fb_query->fbs_statement && !fb_query->out_sqlda) {
            RETVAL_RES(fb_query->res);
            Z_TRY_ADDREF_P(return_value);
            rv = SUCCESS;
            return rv;
        }
    }

	/* Handle result sets for SELECT, EXECUTE PROCEDURE, and DML with RETURNING clauses */
	if (fb_query->out_sqlda) { /* output variables in select, select for update, or RETURNING */

		/* For EXECUTE PROCEDURE and INSERT/UPDATE/DELETE...RETURNING, create independent result resources to avoid
		 * shared state issues where each execution overwrites the previous result.
		 */
  if (fb_query->statement_type == isc_info_sql_stmt_exec_procedure ||
      fb_query->statement_type == isc_info_sql_stmt_insert ||
      fb_query->statement_type == isc_info_sql_stmt_update ||
      fb_query->statement_type == isc_info_sql_stmt_delete) {
			/* Create a new query structure for this specific result */
			fbird_query *result_query = ecalloc(1, sizeof(fbird_query));

			/* Initialize error cleanup flag */
			int cleanup_needed = 1;

            /* Copy essential fields from the original query */
            result_query->link = fb_query->link;
            result_query->trans = fb_query->trans;
            result_query->trans_res = fb_query->trans_res;
            result_query->dialect = fb_query->dialect;
            result_query->statement_type = fb_query->statement_type;
            result_query->out_fields_count = fb_query->out_fields_count;

            /* OO API snapshot support (EXECUTE PROCEDURE + DML RETURNING)
             *
             * The OO API writes result values into a flat message buffer.
             * For one-shot results, we must snapshot that buffer into the
             * returned result resource so subsequent executions don't
             * overwrite it.
             */
            result_query->fbs_statement = fb_query->fbs_statement;
            result_query->out_metadata = fb_query->out_metadata;
            result_query->out_msg_length = fb_query->out_msg_length;
            result_query->out_msg_buffer = NULL;
            if (fb_query->out_msg_buffer && fb_query->out_msg_length > 0) {
                result_query->out_msg_buffer = safe_emalloc(1, fb_query->out_msg_length, 0);
                memcpy(result_query->out_msg_buffer, fb_query->out_msg_buffer, fb_query->out_msg_length);
            }

            /* Flag used by fbird_fetch_*() to detect buffered RETURNING rows.
             * For EXECUTE PROCEDURE it is ignored, but keep it consistent. */
            result_query->was_result_once = fb_query->was_result_once;

            /* Reuse the OO statement wrapper for metadata operations. */
            result_query->fbs_statement = fb_query->fbs_statement;
            /* Keep a copy of SQL text for symmetry with SELECT path and
             * potential debug/logging uses in helper routines. */
            if (fb_query->query) {
                result_query->query = estrdup(fb_query->query);
            }

			/* Validate source SQLDA before processing */
			if (fb_query->out_sqlda && fb_query->out_fields_count > 0) {
				/* Validate SQLDA structure integrity */
				if (fb_query->out_sqlda->sqln != fb_query->out_fields_count ||
				    fb_query->out_sqlda->sqld != fb_query->out_fields_count) {
					_php_fbird_module_error("EXECUTE PROCEDURE: Invalid SQLDA structure - sqln=%d, sqld=%d, expected=%d",
						fb_query->out_sqlda->sqln, fb_query->out_sqlda->sqld, fb_query->out_fields_count);
					goto cleanup_result_query;
				}

				/* Allocate SQLDA structure with bounds checking */
				size_t sqlda_size = XSQLDA_LENGTH(fb_query->out_fields_count);
				if (sqlda_size < sizeof(XSQLDA) || fb_query->out_fields_count > 32767) {
					_php_fbird_module_error("EXECUTE PROCEDURE: Invalid field count %d for SQLDA allocation",
						fb_query->out_fields_count);
					goto cleanup_result_query;
				}

				result_query->out_sqlda = (XSQLDA *) emalloc(sqlda_size);
				if (!result_query->out_sqlda) {
					_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate SQLDA memory");
					goto cleanup_result_query;
				}

				/* Safe copy of SQLDA header and variable array */
				memcpy(result_query->out_sqlda, fb_query->out_sqlda, sqlda_size);

				/* CRITICAL SAFETY FIX: Clear all sqldata pointers in the copy immediately. */
				for (int i = 0; i < fb_query->out_fields_count; i++) {
					result_query->out_sqlda->sqlvar[i].sqldata = NULL;
				}

				/* Allocate null indicator array with validation */
				result_query->out_nullind = safe_emalloc(sizeof(*result_query->out_nullind),
					fb_query->out_fields_count, 0);
				if (!result_query->out_nullind) {
					_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate null indicator array");
					goto cleanup_result_query;
				}

				/* Safe copy of null indicators */
				memcpy(result_query->out_nullind, fb_query->out_nullind,
					sizeof(*result_query->out_nullind) * fb_query->out_fields_count);

				/* Deep copy data for each field using safer copying mechanism */
				for (int i = 0; i < fb_query->out_fields_count; i++) {
					XSQLVAR *orig_var = &fb_query->out_sqlda->sqlvar[i];
					XSQLVAR *result_var = &result_query->out_sqlda->sqlvar[i];

					/* Reset sqldata pointer - will be set by safe copy function */
					result_var->sqldata = NULL;

					/* Use safer copying function with comprehensive validation */
					if (FAILURE == _php_fbird_safe_copy_sqlvar_data(result_var, orig_var, i, fb_query->query)) {
						goto cleanup_result_query;
					}
				}

				/* Update sqlind pointers to point to the new null indicators */
				for (int i = 0; i < fb_query->out_fields_count; i++) {
					if (result_query->out_sqlda->sqlvar[i].sqltype & 1) {
						result_query->out_sqlda->sqlvar[i].sqlind = &result_query->out_nullind[i];
					} else {
						result_query->out_sqlda->sqlvar[i].sqlind = NULL;
					}
				}
			}

   /* Copy input parameter metadata so fbird_num_params()/fbird_param_info()
    * work on the returned result resource (e.g., fbird_query() path). */
   result_query->in_fields_count = fb_query->in_fields_count;
   if (fb_query->in_fields_count > 0 && fb_query->in_sqlda) {
       size_t in_size = XSQLDA_LENGTH(fb_query->in_fields_count);
       result_query->in_sqlda = (XSQLDA *) emalloc(in_size);
       memcpy(result_query->in_sqlda, fb_query->in_sqlda, in_size);
       /* Ensure indicators are not dangling for inputs on the cloned structure */
       for (int i = 0; i < result_query->in_sqlda->sqld; i++) {
           result_query->in_sqlda->sqlvar[i].sqlind = NULL;
           result_query->in_sqlda->sqlvar[i].sqlind = NULL;
       }
   }

   /* Set result flags - EXECUTE PROCEDURE results are immediately available */
   result_query->has_more_rows = 1; /* Data is available for fetching */
   result_query->is_open = 1; /* Result can be fetched once */

   /* Register the result as a new resource - this transfers ownership
    * IMPORTANT: This result does NOT own the statement handle. */
   result_query->owns_stmt_handle = 0;
   result_query->res = zend_register_resource(result_query, le_query);
           if (!result_query->res) {
               _php_fbird_module_error("EXECUTE PROCEDURE: Failed to register result resource");
               goto cleanup_result_query;
           }

            /* Independent result snapshot: DO NOT link as child of parent query. */
            result_query->parent = NULL;

            /* Eagerly load column aliases before clearing the statement handle. */
            if (_php_fbird_alloc_ht_aliases(result_query) == FAILURE) {
                _php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate aliases");
                goto cleanup_result_query;
            }

            result_query->fbs_statement = NULL; /* Do not reference the handle as it may be freed */

			/* Success - disable cleanup since resource system now owns the memory */
			cleanup_needed = 0;

			RETVAL_RES(result_query->res);
			Z_TRY_ADDREF_P(return_value);

			return SUCCESS;

cleanup_result_query:
			/* Clean up partially allocated result_query on error path */
			if (cleanup_needed && result_query) {
				/* Free field data if partially allocated */
				if (result_query->out_sqlda && result_query->out_fields_count > 0) {
					for (int i = 0; i < result_query->out_fields_count; i++) {
						if (result_query->out_sqlda->sqlvar[i].sqldata) {
							efree(result_query->out_sqlda->sqlvar[i].sqldata);
						}
					}
				}

				/* Free SQLDA structure */
				if (result_query->out_sqlda) {
					efree(result_query->out_sqlda);
				}

				/* Free null indicator array */
				if (result_query->out_nullind) {
					efree(result_query->out_nullind);
				}

				/* Free alias hash table if allocated */
				if (result_query->ht_aliases) {
					zend_array_destroy(result_query->ht_aliases);
				}

				/* Free the result query structure itself */
				efree(result_query);
			}

			/* Propagate error to caller */
			goto _php_fbird_ex_error;
  } else {
            /* SELECT queries: Create independent result data to prevent use-after-free vulnerability */

            /* Create a new query structure for this specific result */
            fbird_query *result_query = ecalloc(1, sizeof(fbird_query));

			/* Initialize error cleanup flag */
			int cleanup_needed = 1;

			/* Copy essential fields from the original query */
			result_query->link = fb_query->link;
			result_query->trans = fb_query->trans;
			result_query->trans_res = fb_query->trans_res;
			result_query->dialect = fb_query->dialect;
			result_query->statement_type = fb_query->statement_type;
			result_query->out_fields_count = fb_query->out_fields_count;
			result_query->was_result_once = 1;

   /* Reuse parent's prepared statement and already-open cursor */
   result_query->query = estrdup(fb_query->query);

   /* Copy OO API structures for fetch operations
    *
    * IMPORTANT: Snapshot the message buffer.
    * The parent query owns and frees fb_query->out_msg_buffer.
    * Child SELECT results must have their own copy to avoid double-free / UAF
    * when both parent and child resources are destroyed.
    */
   result_query->fbs_statement = fb_query->fbs_statement;
   result_query->out_metadata = fb_query->out_metadata;
   result_query->out_msg_length = fb_query->out_msg_length;
   result_query->out_msg_buffer = NULL;
   if (fb_query->out_msg_buffer && fb_query->out_msg_length > 0) {
       result_query->out_msg_buffer = safe_emalloc(1, fb_query->out_msg_length, 0);
       memcpy(result_query->out_msg_buffer, fb_query->out_msg_buffer, fb_query->out_msg_length);
   }

   /* Copy input parameter metadata so fbird_num_params()/fbird_param_info()
    * work on the returned result resource (e.g., fbird_query() path). */
   result_query->in_fields_count = fb_query->in_fields_count;
   if (fb_query->in_fields_count > 0 && fb_query->in_sqlda) {
       size_t in_size = XSQLDA_LENGTH(fb_query->in_fields_count);
       result_query->in_sqlda = (XSQLDA *) emalloc(in_size);
       memcpy(result_query->in_sqlda, fb_query->in_sqlda, in_size);
       /* Input buffer pointers are not needed on the result copy when
        * reusing the same open cursor; keep them NULL to avoid misuse. */
       for (int i = 0; i < result_query->in_sqlda->sqld; i++) {
           result_query->in_sqlda->sqlvar[i].sqlind = NULL;
           result_query->in_sqlda->sqlvar[i].sqldata = NULL;
       }
   }

   /* Create independent copies of result data structures */
   if (fb_query->out_fields_count > 0 && fb_query->out_sqlda) {
                /* Validate source SQLDA before processing */
                if (fb_query->out_sqlda->sqln != fb_query->out_fields_count ||
                    fb_query->out_sqlda->sqld != fb_query->out_fields_count) {
                    _php_fbird_module_error("SELECT: Invalid SQLDA structure - sqln=%d, sqld=%d, expected=%d",
                        fb_query->out_sqlda->sqln, fb_query->out_sqlda->sqld, fb_query->out_fields_count);
                    goto cleanup_select_result_query;
                }

				/* Allocate independent SQLDA structure */
				size_t sqlda_size = XSQLDA_LENGTH(fb_query->out_fields_count);
				result_query->out_sqlda = (XSQLDA *) emalloc(sqlda_size);
				if (!result_query->out_sqlda) {
					_php_fbird_module_error("SELECT: Failed to allocate SQLDA memory");
					goto cleanup_select_result_query;
				}

				/* Safe copy of SQLDA header and variable array */
				memcpy(result_query->out_sqlda, fb_query->out_sqlda, sqlda_size);

				/* CRITICAL SAFETY FIX: Clear all sqldata pointers immediately to prevent
				 * double-free of parent data if allocation loop fails. */
				for (int i = 0; i < fb_query->out_fields_count; i++) {
					result_query->out_sqlda->sqlvar[i].sqldata = NULL;
				}

				/* Allocate independent null indicator array */
				result_query->out_nullind = safe_emalloc(sizeof(*result_query->out_nullind),
					fb_query->out_fields_count, 0);
				if (!result_query->out_nullind) {
					_php_fbird_module_error("SELECT: Failed to allocate null indicator array");
					goto cleanup_select_result_query;
				}

				/* Safe copy of null indicators */
				memcpy(result_query->out_nullind, fb_query->out_nullind,
					sizeof(*result_query->out_nullind) * fb_query->out_fields_count);

				/* Copy row buffers only when legacy path populated sqldata.
				 * In OO API mode, out_sqlda is metadata-only and fetch uses out_msg_buffer.
				 */
				for (int i = 0; i < fb_query->out_fields_count; i++) {
					XSQLVAR *orig_var = &fb_query->out_sqlda->sqlvar[i];
					XSQLVAR *result_var = &result_query->out_sqlda->sqlvar[i];

					result_var->sqldata = NULL;

					if (orig_var->sqldata) {
						if (FAILURE == _php_fbird_safe_copy_sqlvar_data(result_var, orig_var, i, fb_query->query)) {
							goto cleanup_select_result_query;
						}
					}
				}

				/* Update sqlind pointers to point to the new null indicators */
				for (int i = 0; i < fb_query->out_fields_count; i++) {
					if (result_query->out_sqlda->sqlvar[i].sqltype & 1) {
						result_query->out_sqlda->sqlvar[i].sqlind = &result_query->out_nullind[i];
					} else {
						result_query->out_sqlda->sqlvar[i].sqlind = NULL;
					}
				}

				/* Copy array metadata if present */
				if (fb_query->out_array_cnt > 0 && fb_query->out_array) {
					result_query->out_array_cnt = fb_query->out_array_cnt;
					result_query->out_array = safe_emalloc(sizeof(fbird_array), fb_query->out_array_cnt, 0);
					if (!result_query->out_array) {
						_php_fbird_module_error("SELECT: Failed to allocate array metadata");
						goto cleanup_select_result_query;
					}
					memcpy(result_query->out_array, fb_query->out_array,
						sizeof(fbird_array) * fb_query->out_array_cnt);
				}
			}

			/* Set result flags - parent query remains open, result has independent data */
			result_query->has_more_rows = 1; /* Data is available for fetching */
			result_query->is_open = 1; /* Result can be fetched */

   /* Register the result as a new resource - this transfers ownership
    * IMPORTANT: This result does NOT own the statement handle. */
   result_query->owns_stmt_handle = 0;
   result_query->res = zend_register_resource(result_query, le_query);
            if (!result_query->res) {
                _php_fbird_module_error("SELECT: Failed to register result resource");
                goto cleanup_select_result_query;
            }

            /* Link this result as a child of the parent prepared query so that
             * freeing the parent can invalidate dependent results (required for
             * use-after-free tests). */
            result_query->parent = fb_query;
            result_query->child_head = NULL;
            result_query->child_next = fb_query->child_head;
            fb_query->child_head = result_query;

   /* NOTE: We do NOT increment parent's refcount. The parent query resource
    * lifetime is controlled by the user, not by child results. This allows
    * multiple fbird_execute() calls on the same prepared query without the
    * query resource becoming invalid after fbird_free_result(). */

   /* Mark cursor state inherited from parent execute */
   result_query->is_open = 1;
   result_query->has_more_rows = 1;

   /* Success - disable cleanup since resource system now owns the memory */
   cleanup_needed = 0;

   RETVAL_RES(result_query->res);
   Z_TRY_ADDREF_P(return_value);

   return SUCCESS;

cleanup_select_result_query:
			/* Clean up partially allocated result_query on error path */
			if (cleanup_needed && result_query) {
				/* Free field data if partially allocated */
				if (result_query->out_sqlda && result_query->out_fields_count > 0) {
					for (int i = 0; i < result_query->out_fields_count; i++) {
						if (result_query->out_sqlda->sqlvar[i].sqldata) {
							efree(result_query->out_sqlda->sqlvar[i].sqldata);
						}
					}
				}

				/* Free SQLDA structure */
				if (result_query->out_sqlda) {
					efree(result_query->out_sqlda);
				}

				/* Free null indicator array */
				if (result_query->out_nullind) {
					efree(result_query->out_nullind);
				}

				/* Free array metadata */
				if (result_query->out_array) {
					efree(result_query->out_array);
				}

				/* Free query string */
				if (result_query->query) {
					efree(result_query->query);
				}

				/* NOTE: Do NOT free result_query->stmt as it's shared with parent */

				/* Free the result query structure itself */
				efree(result_query);
			}

			/* Propagate error to caller */
			goto _php_fbird_ex_error;
		}
	}

	/* Update cursor flags based on statement type and execution result */
	switch (fb_query->statement_type) {

		unsigned long affected_rows;

		case isc_info_sql_stmt_insert:
		case isc_info_sql_stmt_update:
		case isc_info_sql_stmt_delete:
		case isc_info_sql_stmt_exec_procedure:

			affected_rows = 0;

			/* OO API Only: compute affected rows via statement wrapper. */
			if (fb_query->fbs_statement) {
				ISC_UINT64 oo_affected = fbs_get_affected_records(
					FBG(master_instance),
					fb_query->fbs_statement,
					status
				);

				if (FB_STATUS_ERROR(status)) {
					_php_fbird_error(status);
					goto _php_fbird_ex_error;
				}

				affected_rows = (unsigned long)oo_affected;
			} else {
				/* No OO API statement - cannot get affected rows */
				_php_fbird_module_error("Cannot get affected rows: OO API statement required");
				goto _php_fbird_ex_error;
			}

			fb_query->trans->affected_rows = affected_rows;

			if (!fb_query->out_sqlda) { /* no result set is being returned */
				/* Non-SELECT statements without RETURNING clause - no cursor opened */
				fb_query->is_open = 0;
				fb_query->has_more_rows = 0;

				if (affected_rows) {
					RETVAL_LONG(affected_rows);
				} else {
					RETVAL_TRUE;
				}
				break;
			}

			/* DML with RETURNING clause - cursor is opened but handled by result resource */
			fb_query->is_open = 0;
			fb_query->has_more_rows = 0;
			break;

		case isc_info_sql_stmt_select:
		case isc_info_sql_stmt_select_for_upd:
			/* SELECT statements (including SELECT ... FOR UPDATE) - cursor is now open and has potential rows.
			 * Check both legacy (out_sqlda) and OO API (fbs_statement) paths. */
			if (fb_query->out_sqlda || fb_query->fbs_statement) {
				fb_query->is_open = 1;
				fb_query->has_more_rows = 1;
			} else {
				/* SELECT without output - unusual but handle */
				fb_query->is_open = 0;
				fb_query->has_more_rows = 0;
			}
			break;

		default:
			/* Other statement types (DDL, etc.) - no cursor */
			fb_query->is_open = 0;
			fb_query->has_more_rows = 0;
			RETVAL_TRUE;
			break;
	}

	rv = SUCCESS;

_php_fbird_ex_error:
	/* Only clear cursor flags on actual execution error, not on success.
	 * The OO API path sets these flags correctly before reaching here. */
	if (rv == FAILURE) {
		fb_query->is_open = 0;
		fb_query->has_more_rows = 0;
	}
	return rv;
}

PHP_FUNCTION(fbird_query)
{
	ISC_STATUS status[256];
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	char *query = NULL;
	fbird_db_link *link = NULL;
	fbird_transaction *trans = NULL;
	zend_resource *trans_res = NULL;
	fbird_query *fb_query;
	int bind_start = 0;
	int explicit_create = 0;

	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	/* Flexible argument parsing handling optional params and placeholders */
	i = 0;
	while (i < argc) {
		zval *arg = &args[i];
		ZVAL_DEREF(arg); /* Handle references */

		/* Handle FBIRD_CREATE (0) passed as first argument */
		if (i == 0 && Z_TYPE_P(arg) == IS_LONG && Z_LVAL_P(arg) == PHP_FBIRD_CREATE) {
			php_error_docref(NULL, E_DEPRECATED,
				"Passing FBIRD_CREATE to fbird_query() is deprecated, use fbird_create_database() instead");
			explicit_create = 1;
			i++;
			continue;
		}

		if (Z_TYPE_P(arg) == IS_STRING) {
			query = Z_STRVAL_P(arg);
			bind_start = i + 1;
			break;
		} else if (Z_TYPE_P(arg) == IS_RESOURCE || Z_TYPE_P(arg) == IS_OBJECT) {
			/* Phase E: Accept both resources AND Connection/Transaction objects */
			if (!trans && !link) {
				trans = _php_fbird_trans_from_zval(arg, &trans_res);
				if (!trans) {
					link = _php_fbird_link_from_zval(arg);
				}
			} else if (trans && !link) {
				link = _php_fbird_link_from_zval(arg);
			} else if (link && !trans) {
				trans = _php_fbird_trans_from_zval(arg, &trans_res);
			}
		}
		/* Skip non-string, non-resource arguments (e.g. FBIRD_CREATE/0 placeholder) */
		i++;
	}

	if (!query) {
		efree(args);
		_php_fbird_module_error("Query argument missing or not a string");
		RETURN_FALSE;
	}

	/* Handle CREATE DATABASE request via FBIRD_CREATE flag */
	if (explicit_create) {
		unsigned short dialect = 3; /* Default dialect 3 for new databases */

		/* Use OO API for database creation via fbc_create_database() */
		void *create_result = fbc_create_database(
			FBG(master_instance),
			query,
			dialect,
			status
		);

		if (!create_result) {
			_php_fbird_error(status);
			efree(args);
			RETURN_FALSE;
		}

		/* Register the new database connection as a resource.
		 * fbc_create_database returns a CreateDbResult struct containing
		 * the IAttachment* pointer to the newly created database. */
		link = (fbird_db_link *) ecalloc(1, sizeof(fbird_db_link));
		link->dialect = dialect;
		link->tr_list = NULL;
		link->event_head = NULL;

		/* Store the OO API connection wrapper.
		 * The create_result is a pointer that fbc_get_attachment() can use. */
		link->fbc_connection = create_result;

		RETVAL_RES(zend_register_resource(link, le_link));
		efree(args);
		return;
	}

	/* Resolve Link if missing */
	if (!link && !trans) {
		if (FBG(default_link)) {
			link = (fbird_db_link *)zend_fetch_resource2(FBG(default_link), "Firebird link", le_link, le_plink);
		}

		/* If no link found, fail gracefully */
		if (!link) {
			efree(args);
			_php_fbird_module_error("No default connection");
			RETURN_FALSE;
		}
	} else if (!link && trans) {
		/* If transaction is provided but link is not, infer link from transaction.
		   Transactions must be associated with at least one database connection.
		   We default to the first associated link. */
		if (trans->link_cnt > 0) {
			link = trans->db_link[0];
		} else {
			efree(args);
			_php_fbird_module_error("Transaction has no associated link");
			RETURN_FALSE;
		}
	}

	/* Resolve Transaction if missing */
	if (!trans) {
		if (SUCCESS != _php_fbird_def_trans(link, &trans)) {
			efree(args);
			RETURN_FALSE;
		}
	} else {
		/* Explicit transaction passed: must still be active.
		 *
		 * Exception: COMMIT/ROLLBACK on a closed transaction historically yields
		 * a -901 Dynamic SQL Error (not the module_error shortcut).
		 */
		if (trans->fbt_transaction == NULL) {
			const bool is_trans_control_sql =
				strcasecmp(query, "COMMIT") == 0 ||
				strcasecmp(query, "ROLLBACK") == 0 ||
				strcasecmp(query, "COMMIT RETAIN") == 0 ||
				strcasecmp(query, "ROLLBACK RETAIN") == 0;

			efree(args);
			if (is_trans_control_sql) {
				_php_fbird_module_error(
					"Dynamic SQL Error SQL error code = -901 invalid transaction handle (expecting explicit transaction start)"
				);
			} else {
				_php_fbird_module_error("invalid transaction handle (expecting explicit transaction start) ");
			}
			RETURN_FALSE;
		}
	}

	if (!trans) {
		efree(args);
		_php_fbird_module_error("Could not determine transaction");
		RETURN_FALSE;
	}

	if (FAILURE == _php_fbird_prepare(&fb_query, link, trans, trans_res, query)) {
		efree(args);
		RETURN_FALSE;
	}

	for (i = bind_start; i < argc; i++) {
		Z_TRY_ADDREF(args[i]);
	}

	if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, fb_query, &args[bind_start], argc - bind_start)) {
		zend_list_delete(fb_query->res);
		for (i = bind_start; i < argc; i++) {
			zval_ptr_dtor(&args[i]);
		}
		efree(args);
		RETURN_FALSE;
	}

	/* Issue #294: True autocommit for the implicit/default transaction.
	 *
	 * When fbird_query($conn, $sql) is called without an explicit transaction,
	 * _php_fbird_def_trans() provides the cached default transaction. Previously,
	 * this transaction was reused across all autocommit calls without ever being
	 * committed, freezing the snapshot at the time of the first query. Data
	 * committed by other transactions after that point was invisible.
	 *
	 * Fix: for non-SELECT statements (DML/DDL), commit the default transaction
	 * immediately after execution and nullify fbt_transaction so the next
	 * autocommit query starts a fresh transaction with a current snapshot.
	 *
	 * For SELECT statements, the cursor is still open — the default transaction
	 * is committed when the result resource is freed (php_fbird_free_query_rsrc).
	 *
	 * Skip for persistent connections: cleanup_db() may drop the DB during
	 * shutdown, causing MSHUTDOWN crash. The default tx is cleaned up by
	 * _php_fbird_commit_link during MSHUTDOWN (with #295 getMaster() guard).
	 *
	 * trans_res == NULL indicates the default (implicit) transaction was used. */
	{
		bool is_persistent = (link && link->is_persistent);
		if (!trans_res && trans && trans->fbt_transaction &&
			Z_TYPE_P(return_value) != IS_RESOURCE && !is_persistent) {
			/* Issue #294: Commit + free the default transaction for true autocommit.
			 * fbt_free calls rollbackNoThrow() (safe — transaction_ is null after
			 * commit, so it returns early) then deletes the C++ Transaction object.
			 * If commit fails (e.g., open cursors from a prior SELECT on the same
			 * default tx), silently continue — the transaction stays valid and
			 * will be committed at connection close or explicit fbird_commit().
			 * jane: silent on failure — autocommit is an optimization, not a
			 * user-initiated commit; reporting cursor-lock errors would be noise. */
			fbt_commit(trans->fbt_transaction, status);
			fbt_free(trans->fbt_transaction);
			trans->fbt_transaction = NULL;
		}
	}

	if (Z_TYPE_P(return_value) != IS_RESOURCE) {
	    zend_list_delete(fb_query->res);
	} else {
	    /* fbird_query() is one-shot prepare+execute. The parent fb_query is
	     * internal and never exposed to the user. Transfer statement ownership
	     * to the result resource so the parent can be freed immediately.
	     * This prevents server-side prepared statement accumulation when
	     * fbird_query() is called repeatedly (GitHub issue #135). */
	    fbird_query *result_query = (fbird_query *)Z_RES_P(return_value)->ptr;
	    if (result_query && result_query->parent == fb_query) {
	        /* SELECT path: result shares parent's statement handle.
	         * Transfer ownership so result frees it when done. */
	        result_query->owns_stmt_handle = 1;
	        result_query->parent = NULL;
	        fb_query->child_head = NULL;
	        fb_query->fbs_statement = NULL;
	        fb_query->owns_stmt_handle = 0;
	        fb_query->is_open = 0;
	    }
	    /* For EXEC PROCEDURE/DML RETURNING: result has parent=NULL and
	     * fbs_statement=NULL already, so parent dtor will free the statement
	     * via fbs_free() on its own fbs_statement pointer. */
	    zend_list_delete(fb_query->res);
	}

	for (i = bind_start; i < argc; i++) {
		zval_ptr_dtor(&args[i]);
	}
	efree(args);

	/* Issue #296: wrap le_query result in Firebird\ResultSet (same as fbird_execute) */
	if (Z_TYPE_P(return_value) == IS_RESOURCE &&
	    Z_RES_TYPE_P(return_value) == le_query) {
		zend_resource *_res = Z_RES_P(return_value);
		fbird_setup_resultset_object(return_value, _res);
	}
}

PHP_FUNCTION(fbird_prepare)
{
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	char *query = NULL;
	fbird_db_link *link = NULL;
	fbird_transaction *trans = NULL;
	zend_resource *trans_res = NULL;
	fbird_query *fb_query;

	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	/* Phase E: Accept both resources AND Connection/Transaction objects */
	i = 0;
	if (Z_TYPE(args[i]) == IS_RESOURCE || Z_TYPE(args[i]) == IS_OBJECT) {
		trans = _php_fbird_trans_from_zval(&args[i], &trans_res);
		if (trans) {
			i++;
		} else {
			link = _php_fbird_link_from_zval(&args[i]);
			if (link) {
				i++;
			}
		}
	}

	if (i == 1 && i < argc && (Z_TYPE(args[i]) == IS_RESOURCE || Z_TYPE(args[i]) == IS_OBJECT)) {
		if (trans) {
			/* Already have a transaction; next arg could be a link */
			fbird_db_link *l = _php_fbird_link_from_zval(&args[i]);
			if (l) {
				link = l;
				i++;
			}
		} else if (link) {
			/* Already have a link; next arg could be a transaction resource or object */
			fbird_transaction *t = _php_fbird_trans_from_zval(&args[i], &trans_res);
			if (t) {
				trans = t;
				i++;
			}
		}
	}

	if (i < argc && Z_TYPE(args[i]) == IS_STRING) {
		query = Z_STRVAL(args[i]);
	} else {
		efree(args);
		_php_fbird_module_error("Query argument missing or not a string");
		RETURN_FALSE;
	}

	if (!link && !trans) {
		if (FBG(default_link)) {
			link = (fbird_db_link *)zend_fetch_resource2(FBG(default_link), "Firebird link", le_link, le_plink);
		}
		if (!link) {
			efree(args);
			_php_fbird_module_error("No default connection");
			RETURN_FALSE;
		}
	}

	if (!link && trans) {
        /* If transaction is provided but link is not, infer link from transaction.
           Transactions must be associated with at least one database connection.
           We default to the first associated link. */
        if (trans->link_cnt > 0) {
            link = trans->db_link[0];
        } else {
            efree(args);
            _php_fbird_module_error("Transaction has no associated link");
            RETURN_FALSE;
        }
    }

	if (!trans) {
		if (SUCCESS != _php_fbird_def_trans(link, &trans)) {
			efree(args);
			RETURN_FALSE;
		}
	}

	/* cppcheck-suppress legacyUninitvar ; query is guaranteed non-NULL here */
	if (FAILURE == _php_fbird_prepare(&fb_query, link, trans, trans_res, query)) {
		efree(args);
		RETURN_FALSE;
	}

	efree(args);
	RETVAL_RES(fb_query->res);
	Z_TRY_ADDREF_P(return_value);

	/* Issue #297: wrap le_query result in Firebird\Statement object */
	if (Z_TYPE_P(return_value) == IS_RESOURCE &&
	    Z_RES_TYPE_P(return_value) == le_query) {
		zend_resource *_res = Z_RES_P(return_value);
		fbird_setup_statement_object(return_value, _res);
	}
}

/* {{{ proto resource fbird_prepare_ex(resource $link, string $query [, resource $trans])
   Prepare a statement with a fixed, non-shifting signature */
PHP_FUNCTION(fbird_prepare_ex)
{
	zval *link_arg = NULL, *trans_arg = NULL;
	char *query = NULL;
	size_t query_len;
	fbird_db_link *link = NULL;
	fbird_transaction *trans = NULL;
	zend_resource *trans_res = NULL;
	fbird_query *fb_query;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs|z!",
			&link_arg, &query, &query_len, &trans_arg) == FAILURE) {
		return;
	}

	/* Resolve link: accept both legacy resource and Firebird\Connection object */
	if (Z_TYPE_P(link_arg) == IS_OBJECT &&
	    instanceof_function(Z_OBJCE_P(link_arg), fbird_connection_ce)) {
		zend_resource *cres = fbird_connection_get_resource(Z_OBJ_P(link_arg));
		if (!cres || !cres->ptr) {
			_php_fbird_module_error("Firebird\\Connection object has no valid resource");
			RETURN_FALSE;
		}
		link = (fbird_db_link *)cres->ptr;
	} else {
		link = (fbird_db_link *)zend_fetch_resource_ex(link_arg, NULL, le_link);
		if (!link) {
			link = (fbird_db_link *)zend_fetch_resource_ex(link_arg, NULL, le_plink);
		}
		if (!link) {
			_php_fbird_module_error("First argument must be a Firebird connection resource");
			RETURN_FALSE;
		}
	}

	/* Resolve optional transaction - accept both resource and Transaction object */
	if (trans_arg) {
		trans = _php_fbird_trans_from_zval(trans_arg, &trans_res);
	}

	if (!trans) {
		if (SUCCESS != _php_fbird_def_trans(link, &trans)) {
			RETURN_FALSE;
		}
	}

	if (FAILURE == _php_fbird_prepare(&fb_query, link, trans, trans_res, query)) {
		RETURN_FALSE;
	}

	RETVAL_RES(fb_query->res);
	Z_TRY_ADDREF_P(return_value);

	/* Issue #297: wrap le_query result in Firebird\Statement object */
	if (Z_TYPE_P(return_value) == IS_RESOURCE &&
	    Z_RES_TYPE_P(return_value) == le_query) {
		zend_resource *_res = Z_RES_P(return_value);
		fbird_setup_statement_object(return_value, _res);
	}
}
/* }}} */

PHP_FUNCTION(fbird_execute)
{
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	fbird_query *fb_query;

	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	/* Validate first argument: query resource OR Firebird\ResultSet/Statement object.
	 * Throw TypeError for wrong types (consistent with other fbird_* functions).
	 * Issue #297: also accept Firebird\Statement (returned by fbird_prepare/ex). */
	if (Z_TYPE(args[0]) != IS_RESOURCE &&
	    !(Z_TYPE(args[0]) == IS_OBJECT &&
	      (instanceof_function(Z_OBJCE(args[0]), fbird_resultset_ce) ||
	       instanceof_function(Z_OBJCE(args[0]), fbird_statement_ce)))) {
		/* Capture type name BEFORE efree(args) to avoid use-after-free */
		const char *arg_type = zend_get_type_by_const(Z_TYPE(args[0]));
		zend_type_error("fbird_execute(): Argument #1 ($query) must be a Firebird query resource or Firebird\\ResultSet/Statement, %s given", arg_type);
		efree(args);
		RETURN_THROWS();
	}

	FBIRD_VALIDATE_QUERY_EX(&args[0], 1, fb_query);
	if (!fb_query) {
		efree(args);
		RETURN_FALSE;
	}

	for (i = 1; i < argc; i++) {
		Z_TRY_ADDREF(args[i]);
	}

	if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, fb_query, &args[1], argc - 1)) {
		for (i = 1; i < argc; i++) {
			zval_ptr_dtor(&args[i]);
		}
		efree(args);
		RETURN_FALSE;
	}

	for (i = 1; i < argc; i++) {
		zval_ptr_dtor(&args[i]);
	}
	efree(args);

	/* M3 Phase G: wrap le_query result in Firebird\ResultSet */
	if (Z_TYPE_P(return_value) == IS_RESOURCE &&
	    Z_RES_TYPE_P(return_value) == le_query) {
		zend_resource *_res = Z_RES_P(return_value);
		fbird_setup_resultset_object(return_value, _res);
	}
}

void _php_fbird_free_query_impl(INTERNAL_FUNCTION_PARAMETERS, int as_result)
{
	zval *query_arg;
	fbird_query *fb_query;
	zend_resource *res;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &query_arg) == FAILURE) {
		return;
	}

	/* M3 Phase G: Accept Firebird\ResultSet and Firebird\Statement objects */
	if (Z_TYPE_P(query_arg) == IS_OBJECT &&
	    instanceof_function(Z_OBJCE_P(query_arg), fbird_resultset_ce)) {
		res = fbird_resultset_get_resource(Z_OBJ_P(query_arg));
		if (!res) {
			RETURN_FALSE;
		}
	} else if (Z_TYPE_P(query_arg) == IS_OBJECT &&
	           instanceof_function(Z_OBJCE_P(query_arg), fbird_statement_ce)) {
		/* Issue #297: Firebird\Statement returned by fbird_prepare()/fbird_prepare_ex() */
		res = fbird_statement_get_resource(Z_OBJ_P(query_arg));
		if (!res) {
			RETURN_FALSE;
		}
	} else {
		if (Z_TYPE_P(query_arg) != IS_RESOURCE) {
			const char *arg_type = zend_get_type_by_const(Z_TYPE_P(query_arg));
			zend_type_error("fbird_free_query(): Argument #1 ($query) must be a Firebird query resource or Firebird\\ResultSet/Statement, %s given", arg_type);
			RETURN_THROWS();
		}
		res = Z_RES_P(query_arg);
	}

	fb_query = (fbird_query *)zend_fetch_resource(res, "Firebird query", le_query);
	if (!fb_query) {
		RETURN_FALSE;
	}

	zend_list_close(res);
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_free_query)
{
	_php_fbird_free_query_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}

PHP_FUNCTION(fbird_affected_rows)
{
	zval *link_arg = NULL;
	fbird_db_link *link = NULL;
	fbird_transaction *trans = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!", &link_arg) == FAILURE) {
		return;
	}

	if (link_arg) {
		FBIRD_VALIDATE_LINK_EX(link_arg, 1, link);
	} else {
		if (FBG(default_link)) {
			link = (fbird_db_link *)zend_fetch_resource2(FBG(default_link), "Firebird link", le_link, le_plink);
		}
	}

	if (!link) {
		RETURN_FALSE;
	}

	if (SUCCESS == _php_fbird_def_trans(link, &trans)) {
		RETVAL_LONG(trans->affected_rows);
	} else {
		RETURN_FALSE;
	}
}

static zval * _php_fbird_hash_to_zval_array(HashTable *ht, int *count)
{
    int n = zend_hash_num_elements(ht);
    if (n == 0) {
        *count = 0;
        return NULL;
    }
    zval *arr = safe_emalloc(n, sizeof(zval), 0);
    int i = 0;
    zval *entry;
    ZEND_HASH_FOREACH_VAL(ht, entry) {
        ZVAL_COPY(&arr[i], entry);
        i++;
    } ZEND_HASH_FOREACH_END();
    *count = n;
    return arr;
}

PHP_FUNCTION(fbird_execute_statement)
{
    zval *trans_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    fbird_transaction *trans;
    fbird_db_link *link = NULL;
    fbird_query *fb_query;
    zval *bind_args = NULL;
    int bind_n = 0;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs|a!", &trans_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    FBIRD_VALIDATE_TRANS_EX(trans_arg, 1, trans);
    if (trans->link_cnt > 0) {
        link = trans->db_link[0];
    } else {
        _php_fbird_module_error("Transaction has no associated link");
        RETURN_FALSE;
    }

    if (FAILURE == _php_fbird_prepare(&fb_query, link, trans, Z_RES_P(trans_arg), sql)) {
        RETURN_FALSE;
    }

    if (params_arg && Z_TYPE_P(params_arg) == IS_ARRAY) {
        bind_args = _php_fbird_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, fb_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }
        zend_list_delete(fb_query->res);
        RETURN_FALSE;
    }

    if (bind_args) {
        for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
        efree(bind_args);
    }

    /* Strict return type check */
    if (Z_TYPE_P(return_value) == IS_RESOURCE) {
        zend_throw_error(NULL, "fbird_execute_statement expects a DML/DDL statement, but SELECT was executed. Use fbird_execute_query().");
        zend_list_delete(Z_RES_P(return_value));
        zend_list_delete(fb_query->res);
        RETURN_THROWS();
    }

    /* One-shot: prepare + execute + destroy. Use fbird_prepare/fbird_execute for reuse. */
    zend_list_delete(fb_query->res);

    if (Z_TYPE_P(return_value) == IS_TRUE) {
        RETVAL_LONG(0);
    }
}

PHP_FUNCTION(fbird_execute_query)
{
    zval *trans_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    fbird_transaction *trans;
    fbird_db_link *link = NULL;
    fbird_query *fb_query;
    zval *bind_args = NULL;
    int bind_n = 0;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs|a!", &trans_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    FBIRD_VALIDATE_TRANS_EX(trans_arg, 1, trans);
    if (trans->link_cnt > 0) {
        link = trans->db_link[0];
    } else {
        _php_fbird_module_error("Transaction has no associated link");
        RETURN_FALSE;
    }

    /* cppcheck-suppress legacyUninitvar ; link is guaranteed non-NULL here */
    if (FAILURE == _php_fbird_prepare(&fb_query, link, trans, Z_RES_P(trans_arg), sql)) {
        RETURN_FALSE;
    }

    if (params_arg && Z_TYPE_P(params_arg) == IS_ARRAY) {
        bind_args = _php_fbird_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, fb_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }
        zend_list_delete(fb_query->res);
        RETURN_FALSE;
    }

    if (bind_args) {
        for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
        efree(bind_args);
    }

    if (Z_TYPE_P(return_value) != IS_RESOURCE) {
        /* RETURNING queries also return resource if execute2 results are present. */
        zend_throw_error(NULL, "fbird_execute_query expects a SELECT or RETURNING statement.");
        // _php_fbird_exec returns TRUE/LONG for DML.
        zend_list_delete(fb_query->res);
        RETURN_THROWS();
    }

    /* One-shot prepare+execute: transfer statement ownership to result and
     * free the internal parent immediately (GitHub issue #135). */
    {
        fbird_query *result_query = (fbird_query *)Z_RES_P(return_value)->ptr;
        if (result_query && result_query->parent == fb_query) {
            result_query->owns_stmt_handle = 1;
            result_query->parent = NULL;
            fb_query->child_head = NULL;
            fb_query->fbs_statement = NULL;
            fb_query->owns_stmt_handle = 0;
            fb_query->is_open = 0;
        }
        zend_list_delete(fb_query->res);
    }

	/* Issue #296: wrap le_query result in Firebird\ResultSet (same as fbird_execute) */
	if (Z_TYPE_P(return_value) == IS_RESOURCE &&
	    Z_RES_TYPE_P(return_value) == le_query) {
		zend_resource *_res = Z_RES_P(return_value);
		fbird_setup_resultset_object(return_value, _res);
	}
}

PHP_FUNCTION(fbird_execute_auto)
{
	ISC_STATUS status[256];
    zval *link_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    fbird_db_link *link;
    fbird_transaction *trans;
    fbird_query *fb_query;
    zval *bind_args = NULL;
    int bind_n = 0;
    void *oo_trans = NULL;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs|a!", &link_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    FBIRD_VALIDATE_LINK_EX(link_arg, 1, link);

    /* OO API Only: Connection must have OO API handle */
    if (!link->fbc_connection) {
        _php_fbird_module_error("fbird_execute_auto requires OO API connection (fbc_connection required)");
        RETURN_FALSE;
    }

    /* Start autonomous transaction via OO API */
    void *attachment = fbc_get_attachment(link->fbc_connection);
    oo_trans = fbt_start(FBG(master_instance), attachment, 0, NULL, status);
    if (!oo_trans) {
        _php_fbird_error(status);
        RETURN_FALSE;
    }

    /* Create temp trans object with OO API transaction */
    trans = (fbird_transaction *) emalloc(sizeof(fbird_transaction));
    trans->link_cnt = 1;
    trans->affected_rows = 0;
    trans->fbt_transaction = oo_trans;
    trans->db_link[0] = link;
    /* We do NOT register this transaction as a resource because it's strictly local scope */

    /* Prepare */
    if (FAILURE == _php_fbird_prepare(&fb_query, link, trans, NULL, sql)) {
        fbt_rollback(oo_trans, status);
        efree(trans);
        RETURN_FALSE;
    }

    if (params_arg && Z_TYPE_P(params_arg) == IS_ARRAY) {
        bind_args = _php_fbird_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, fb_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }

        zend_list_delete(fb_query->res); // Frees statement
        fbt_rollback(oo_trans, status);
        efree(trans);
        RETURN_FALSE;
    }

    if (bind_args) {
        for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
        efree(bind_args);
    }

    /* Check if it returned a resource (SELECT) */
    if (Z_TYPE_P(return_value) == IS_RESOURCE) {
        /* We cannot support returning a cursor from an autonomous transaction
           because we commit immediately below, which would close the cursor. */
        zend_throw_error(NULL, "fbird_execute_auto cannot be used with SELECT statements (cursor would be closed on commit).");
        zend_list_delete(Z_RES_P(return_value));
        zend_list_delete(fb_query->res);
        fbt_rollback(oo_trans, status);
        efree(trans);
        RETURN_THROWS();
    }

    /* Free statement BEFORE committing transaction to avoid
     * fbs_free() issues when the transaction is already gone.
     * Save the result value before deleting the query resource. */
    zval saved_result;
    ZVAL_COPY_VALUE(&saved_result, return_value);
    zend_list_delete(fb_query->res);

    /* Commit via OO API (returns 0 on success, non-zero on error) */
    if (fbt_commit(oo_trans, status)) {
        _php_fbird_error(status);
        efree(trans);
        RETURN_FALSE;
    }

    efree(trans);

    /* Restore return value (affected rows count) */
    ZVAL_COPY_VALUE(return_value, &saved_result);

    /* Return value is already set by _php_fbird_exec (TRUE/affected_rows) */
}

int _php_fbird_fetch_query_res(zval *from, fbird_query **fb_query)
{
	if (Z_TYPE_P(from) != IS_RESOURCE) {
		return 0;
	}
	*fb_query = (fbird_query *)zend_fetch_resource_ex(from, "Firebird query", le_query);
	return (*fb_query) ? 1 : 0;
}

/**
 * fbird_query_params_tx(resource $link, resource $trans, string $query[, array $params]): resource|int|bool
 *
 * Execute a parameterized query with explicit link AND transaction handles.
 * Required by doctrine-firebird-driver which passes both handles explicitly
 * (unlike fbird_execute_query which infers the link from the transaction).
 *
 * Combines fbird_prepare() + fbird_execute() in one call for performance.
 * Returns result resource for SELECT, affected-row count for DML, or false on error.
 */
PHP_FUNCTION(fbird_query_params_tx)
{
    zval *link_arg, *trans_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    fbird_db_link *link;
    fbird_transaction *trans;
    fbird_query *fb_query;
    zval *bind_args = NULL;
    int bind_n = 0;

    RESET_ERRMSG;

    /* Accept: link, trans, sql [, params_array] */
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "zzs|a!",
            &link_arg, &trans_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    FBIRD_VALIDATE_LINK_EX(link_arg, 1, link);
    FBIRD_VALIDATE_TRANS_EX(trans_arg, 2, trans);

    if (FAILURE == _php_fbird_prepare(&fb_query, link, trans, Z_RES_P(trans_arg), sql)) {
        RETURN_FALSE;
    }

    if (params_arg && Z_TYPE_P(params_arg) == IS_ARRAY) {
        bind_args = _php_fbird_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, fb_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i = 0; i < bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }
        zend_list_delete(fb_query->res);
        RETURN_FALSE;
    }

    if (bind_args) {
        for (int i = 0; i < bind_n; i++) zval_ptr_dtor(&bind_args[i]);
        efree(bind_args);
    }

    /* One-shot prepare+execute: free parent immediately. For result resources,
     * transfer statement ownership to prevent server-side leak (#135). */
    if (Z_TYPE_P(return_value) != IS_RESOURCE) {
        zend_list_delete(fb_query->res);
    } else {
        fbird_query *result_query = (fbird_query *)Z_RES_P(return_value)->ptr;
        if (result_query && result_query->parent == fb_query) {
            result_query->owns_stmt_handle = 1;
            result_query->parent = NULL;
            fb_query->child_head = NULL;
            fb_query->fbs_statement = NULL;
            fb_query->owns_stmt_handle = 0;
            fb_query->is_open = 0;
        }
        zend_list_delete(fb_query->res);
    }

	/* Issue #296: wrap le_query result in Firebird\ResultSet (same as fbird_execute) */
	if (Z_TYPE_P(return_value) == IS_RESOURCE &&
	    Z_RES_TYPE_P(return_value) == le_query) {
		zend_resource *_res = Z_RES_P(return_value);
		fbird_setup_resultset_object(return_value, _res);
	}
}

/* #372: fbird_multi_query - execute multiple statements separated by ; */
PHP_FUNCTION(fbird_multi_query)
{
	zval *link_arg = NULL;
	char *sql;
	size_t sql_len;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zs", &link_arg, &sql, &sql_len) == FAILURE) {
		RETURN_THROWS();
	}

	/* Split SQL on ; and execute each statement */
	char *p = sql;
	char *start = sql;
	bool first = true;

	while (p <= sql + sql_len) {
		if (*p == ';' || p == sql + sql_len) {
			size_t stmt_len = p - start;
			/* Skip empty statements (leading/trailing/duplicate semicolons) */
			while (stmt_len > 0 && isspace((unsigned char)*start)) { start++; stmt_len--; }
			while (stmt_len > 0 && isspace((unsigned char)start[stmt_len-1])) stmt_len--;

			if (stmt_len > 0) {
				zval fn_name, sql_zv, query_ret;
				zval args[2];
				ZVAL_STRING(&fn_name, "fbird_query");
				ZVAL_STRINGL(&sql_zv, start, stmt_len);
				args[0] = *link_arg;
				args[1] = sql_zv;
				call_user_function(EG(function_table), NULL, &fn_name, &query_ret, 2, args);
				zval_ptr_dtor(&fn_name);
				zval_ptr_dtor(&sql_zv);

				if (Z_TYPE(query_ret) == IS_FALSE) {
					RETURN_FALSE;
				}
				if (first) {
					RETVAL_COPY_VALUE(&query_ret);
					first = false;
				} else {
					zval_ptr_dtor(&query_ret);
				}
			}
			start = p + 1;
		}
		p++;
	}

	if (first) {
		RETURN_TRUE;  /* No statements to execute */
	}
}

/* #378: fbird_stmt_attr_get - get statement attribute */
PHP_FUNCTION(fbird_stmt_attr_get)
{
	zval *stmt_arg;
	zend_long attr;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &stmt_arg, &attr) == FAILURE) {
		RETURN_THROWS();
	}

	/* Map generic attributes to FB-specific getters */
	switch (attr) {
#if FB_API_VER >= 40
		case 1026: /* FBIRD_ATTR_STATEMENT_TIMEOUT */
		{
			zval fn_name, ret;
			ZVAL_STRING(&fn_name, "fbird_stmt_get_timeout");
			call_user_function(EG(function_table), NULL, &fn_name, &ret, 1, stmt_arg);
			zval_ptr_dtor(&fn_name);
			RETURN_COPY_VALUE(&ret);
		}
#endif
		default:
			_php_fbird_module_error("Unknown statement attribute %ld", (long)attr);
			RETURN_FALSE;
	}
}

/* #378: fbird_stmt_attr_set - set statement attribute */
PHP_FUNCTION(fbird_stmt_attr_set)
{
	zval *stmt_arg;
	zend_long attr;
	zval *value;
	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zlz", &stmt_arg, &attr, &value) == FAILURE) {
		RETURN_THROWS();
	}

	switch (attr) {
#if FB_API_VER >= 40
		case 1026: /* FBIRD_ATTR_STATEMENT_TIMEOUT */
		{
			zval fn_name, ret;
			zval args[2] = {*stmt_arg, *value};
			ZVAL_STRING(&fn_name, "fbird_stmt_set_timeout");
			call_user_function(EG(function_table), NULL, &fn_name, &ret, 2, args);
			zval_ptr_dtor(&fn_name);
			if (Z_TYPE(ret) == IS_FALSE) {
				zval_ptr_dtor(&ret);
				RETURN_FALSE;
			}
			zval_ptr_dtor(&ret);
			RETURN_TRUE;
		}
#endif
		default:
			_php_fbird_module_error("Unknown statement attribute %ld", (long)attr);
			RETURN_FALSE;
	}
}

#endif /* HAVE_FIREBIRD */

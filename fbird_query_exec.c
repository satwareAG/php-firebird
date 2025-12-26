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
#include "src/php_fbird_compat.h"

/* le_query is defined in fbird_query_prepare.c */

static int _php_fbird_exec(INTERNAL_FUNCTION_PARAMETERS, fbird_query *ib_query, zval *args, int bind_n) /* {{{ */
{
	int rv = FAILURE;
	ISC_STATUS isc_result;
	int argc = ib_query->in_fields_count;

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
		php_error_docref(NULL, E_WARNING, "Invalid parameter count: bind_n=%d, argc=%d", bind_n, argc);
		return FAILURE;
	}

	if (bind_n != argc) {
		php_error_docref(NULL, (bind_n < argc) ? E_WARNING : E_NOTICE,
			"Statement expects %d arguments, %d given", argc, bind_n);

		if (bind_n < argc) {
			return FAILURE;
		}
	}

 /* Cursor lifecycle management: before any re-execution, close an open cursor
  * unconditionally to match legacy semantics and avoid -502 reopen errors. */
 if (ib_query->statement_type != isc_info_sql_stmt_exec_procedure && ib_query->is_open) {
     FBDEBUG("Closing open cursor before re-execution");
     /* Be tolerant: silently ignore ALL errors when attempting to close the cursor
      * before re-execution. The cursor may already have been closed by various means
      * (fbird_free_result, transaction commit, EOF reached, etc.) - this is expected
      * and should not generate warnings. We unconditionally reset the is_open flag. */

     /* OO API Only: Close cursor via fbs_close_cursor() */
     if (ib_query->fbs_statement) {
         fbs_close_cursor(ib_query->fbs_statement, IB_STATUS);
     }
     ib_query->is_open = 0;
     ib_query->has_more_rows = 0;
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

	switch (ib_query->statement_type) {
		fb_safe_handle tr;
		fbird_tr_list **l;
		fbird_transaction *trans;

		case isc_info_sql_stmt_start_trans:

			/* OO API path: Use fbt_start() for OO API connections.
			 * This avoids calling isc_dsql_execute_immediate() with invalid legacy handles.
			 * Note: SET TRANSACTION SQL parameters are not parsed here - uses default TPB. */
			if (ib_query->link && ib_query->link->fbc_connection) {
				void *attachment = fbc_get_attachment(ib_query->link->fbc_connection);
				void *new_trans = NULL;

				FBDEBUG("OO API: Executing SET TRANSACTION via fbt_start()");

				/* Start transaction with default TPB (READ_WRITE, WAIT, CONCURRENCY) */
				new_trans = fbt_start(IBG(master_instance), attachment, 0, NULL, IB_STATUS);
				if (!new_trans) {
					_php_fbird_error();
					goto _php_fbird_ex_error;
				}

				trans = (fbird_transaction *) emalloc(sizeof(fbird_transaction));
				trans->handle.ptr = NULL; /* No legacy handle for OO API transaction */
				trans->link_cnt = 1;
				trans->affected_rows = 0;
				trans->fbt_transaction = new_trans;
				trans->db_link[0] = ib_query->link;

				if (ib_query->link->tr_list == NULL) {
					ib_query->link->tr_list = (fbird_tr_list *) emalloc(sizeof(fbird_tr_list));
					ib_query->link->tr_list->trans = NULL;
					ib_query->link->tr_list->next = NULL;
				}

				/* link the transaction into the connection-transaction list */
				for (l = &ib_query->link->tr_list; *l != NULL; l = &(*l)->next);
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
			if (ib_query->trans && ib_query->trans->fbt_transaction) {
				int rc;
				if (ib_query->statement_type == isc_info_sql_stmt_commit) {
					FBDEBUG("OO API: Executing COMMIT via fbt_commit()");
					rc = fbt_commit(ib_query->trans->fbt_transaction, IB_STATUS);
				} else {
					FBDEBUG("OO API: Executing ROLLBACK via fbt_rollback()");
					rc = fbt_rollback(ib_query->trans->fbt_transaction, IB_STATUS);
				}
				if (rc != 0) {
					_php_fbird_error();
					goto _php_fbird_ex_error;
				}

				/* Mark transaction as closed.
				 * Note: Do NOT delete the PHP resource here.
				 * Legacy behavior keeps the resource alive but invalid for further use.
				 */
				ib_query->trans->fbt_transaction = NULL;
				ib_query->trans->handle.ptr = 0;

				RETVAL_TRUE;
				return SUCCESS;
			}

			/* OO API Only: Transaction must have OO API handle for COMMIT/ROLLBACK */
			_php_fbird_module_error("COMMIT/ROLLBACK requires OO API transaction (fbt_transaction required)");
			goto _php_fbird_ex_error;

		default:
			RETVAL_FALSE;
	}

	if (ib_query->in_fields_count) { /* has placeholders */
		FBDEBUG("Query wants XSQLDA for input");
		if (_php_fbird_bind(ib_query, args) == FAILURE) {
			FBDEBUG("Could not bind input XSQLDA");
			goto _php_fbird_ex_error;
		}

		/* Verify OO API message buffer infrastructure is available.
		 * If in_metadata or in_msg_buffer is not set, OO API execution
		 * with parameters is not possible - report clear error. */
		if (!ib_query->in_metadata) {
			_php_fbird_module_error("OO API input metadata not available for parameterized query");
			goto _php_fbird_ex_error;
		}
		if (!ib_query->in_msg_buffer) {
			_php_fbird_module_error("OO API input message buffer not allocated for parameterized query");
			goto _php_fbird_ex_error;
		}

		/* Transfer bound XSQLDA values to OO API message buffer.
		 * This is required because the OO API uses flat message buffers
		 * with offsets from IMessageMetadata, not XSQLDA structures. */
		if (_php_fbird_xsqlda_to_msg_buffer(ib_query) == FAILURE) {
			FBDEBUG("Could not transfer XSQLDA to message buffer");
			goto _php_fbird_ex_error;
		}
	}

    /* Execute the statement. For SELECT, this opens the cursor on ib_query->stmt. */

    /*
     * Phase 5 Part 3: OO API execution path for prepared statements.
     *
     * When fbs_statement is set (OO API statement prepared via fbs_prepare),
     * we attempt execution through the modern OO API before falling back to
     * the legacy isc_dsql_execute()/isc_dsql_execute2() path.
     *
     * Current limitations:
     * - Input/output message buffers passed as NULL (XSQLDA not converted yet)
     * - Works for simple non-parameterized statements
     * - Parameterized queries continue to use legacy XSQLDA binding
     *
     * The OO API execution is preferred for SELECT statements (cursor operations)
     * and simple DML without parameters. Complex parameterized queries fall back
     * to the legacy path until full message buffer integration is implemented.
     */
    isc_result = 0; /* Initialize for OO API path which may skip legacy execution */

    if (ib_query->fbs_statement && ib_query->trans && ib_query->trans->fbt_transaction) {
        void *transaction_ptr = fbt_get_handle(ib_query->trans->fbt_transaction);
        int oo_api_success = 0;

        if (transaction_ptr) {
            /* For SELECT statements, open cursor using OO API */
            if (ib_query->statement_type == isc_info_sql_stmt_select ||
                ib_query->statement_type == isc_info_sql_stmt_select_for_upd) {
                /*
                 * OO API cursor open for SELECT.
                 * Uses IMessageMetadata for parameter binding (Firebird 3.0+ OO API).
                 * Input parameters are passed via in_msg_buffer/in_metadata.
                 */
                oo_api_success = fbs_open_cursor(
                    IBG(master_instance),
                    ib_query->fbs_statement,
                    transaction_ptr,
                    ib_query->in_msg_buffer,  /* in_msg: parameter values */
                    ib_query->in_metadata,    /* in_metadata: parameter metadata */
                    0,    /* cursor_flags: default */
                    IB_STATUS
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_open_cursor() succeeded for SELECT");
                    isc_result = 0; /* Success */
                } else {
                    /* OO API cursor open failed - report error immediately, no fallback */
                    _php_fbird_error();
                    goto _php_fbird_ex_error;
                }
            }
            /* For non-SELECT (INSERT/UPDATE/DELETE) without RETURNING, use fbs_execute */
            else if ((ib_query->statement_type == isc_info_sql_stmt_insert ||
                      ib_query->statement_type == isc_info_sql_stmt_update ||
                      ib_query->statement_type == isc_info_sql_stmt_delete) &&
                     !ib_query->out_sqlda) {
                /*
                 * OO API execute for DML (no RETURNING).
                 * Uses IMessageMetadata for parameter binding (Firebird 3.0+ OO API).
                 * Input parameters are passed via in_msg_buffer/in_metadata.
                 */
                oo_api_success = fbs_execute(
                    IBG(master_instance),
                    ib_query->fbs_statement,
                    transaction_ptr,
                    ib_query->in_msg_buffer,  /* in_msg: parameter values */
                    ib_query->in_metadata,    /* in_metadata: parameter metadata */
                    NULL, /* out_msg: no output for DML without RETURNING */
                    NULL, /* out_metadata: no output for DML without RETURNING */
                    IB_STATUS
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_execute() succeeded for DML");
                    isc_result = 0; /* Success */
                } else {
                    /* OO API execution failed - report error immediately, no fallback */
                    _php_fbird_error();
                    goto _php_fbird_ex_error;
                }
            }
            /* For DDL statements (CREATE, DROP, ALTER, etc.), use fbs_execute */
            else if (ib_query->statement_type == isc_info_sql_stmt_ddl) {
                /*
                 * DDL statements have no input/output parameters.
                 * Execute directly via OO API.
                 */
                oo_api_success = fbs_execute(
                    IBG(master_instance),
                    ib_query->fbs_statement,
                    transaction_ptr,
                    NULL, /* in_msg */
                    NULL, /* in_metadata */
                    NULL, /* out_msg */
                    NULL, /* out_metadata */
                    IB_STATUS
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_execute() succeeded for DDL");
                    isc_result = 0; /* Success */
                } else {
                    /* OO API execution failed - report error immediately, no fallback */
                    _php_fbird_error();
                    goto _php_fbird_ex_error;
                }
            }
            /* SAVEPOINT statements (SAVEPOINT / ROLLBACK TO SAVEPOINT / RELEASE SAVEPOINT)
             * are reported as statement type 14. They have no input/output parameters.
             */
            else if (ib_query->statement_type == isc_info_sql_stmt_savepoint) {
                oo_api_success = fbs_execute(
                    IBG(master_instance),
                    ib_query->fbs_statement,
                    transaction_ptr,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    IB_STATUS
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_execute() succeeded for SAVEPOINT");
                    isc_result = 0;
                } else {
                    _php_fbird_error();
                    goto _php_fbird_ex_error;
                }
            }
            /* For EXECUTE PROCEDURE - use fbs_execute with input and output buffers */
            else if (ib_query->statement_type == isc_info_sql_stmt_exec_procedure) {
                /*
                 * OO API execute for EXECUTE PROCEDURE.
                 * Uses IMessageMetadata for parameter binding (Firebird 3.0+ OO API).
                 * Input parameters are passed via in_msg_buffer/in_metadata.
                 * Output parameters are returned via out_msg_buffer/out_metadata.
                 */
                oo_api_success = fbs_execute(
                    IBG(master_instance),
                    ib_query->fbs_statement,
                    transaction_ptr,
                    ib_query->in_msg_buffer,   /* in_msg: input parameter values */
                    ib_query->in_metadata,     /* in_metadata: input parameter metadata */
                    ib_query->out_msg_buffer,  /* out_msg: output parameter values */
                    ib_query->out_metadata,    /* out_metadata: output parameter metadata */
                    IB_STATUS
                );
                if (oo_api_success) {
                    FBDEBUG("OO API fbs_execute() succeeded for EXECUTE PROCEDURE");
                    isc_result = 0; /* Success */
                } else {
                    /* OO API execution failed - report error immediately, no fallback */
                    _php_fbird_error();
                    goto _php_fbird_ex_error;
                }
            }
            /* For DML with RETURNING - open cursor, fetch 1 row into out_msg_buffer, close cursor */
            else if ((ib_query->statement_type == isc_info_sql_stmt_insert ||
                      ib_query->statement_type == isc_info_sql_stmt_update ||
                      ib_query->statement_type == isc_info_sql_stmt_delete) &&
                     ib_query->out_sqlda) {
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
                    IBG(master_instance),
                    ib_query->fbs_statement,
                    transaction_ptr,
                    ib_query->in_msg_buffer,
                    ib_query->in_metadata,
                    0,
                    IB_STATUS
                );

                if (!oo_api_success) {
                    _php_fbird_error();
                    goto _php_fbird_ex_error;
                }

                int fetch_result = fbs_fetch(
                    IBG(master_instance),
                    ib_query->fbs_statement,
                    ib_query->out_msg_buffer,
                    IB_STATUS
                );

                if (fetch_result == -1) {
                    _php_fbird_error();
                    fbs_close_cursor(ib_query->fbs_statement, IB_STATUS);
                    goto _php_fbird_ex_error;
                }

                /* Always close cursor for DML RETURNING (like execute2) */
                fbs_close_cursor(ib_query->fbs_statement, IB_STATUS);

                /* fetch_result: 1=row copied into out_msg_buffer, 0=no data */
                if (fetch_result == 1) {
                    ib_query->was_result_once = 1;
                }
                isc_result = 0;
            }
            /* Unhandled statement type for OO API */
            else {
                _php_fbird_module_error("Statement type %d not supported via OO API",
                    ib_query->statement_type);
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
        _php_fbird_error();
        goto _php_fbird_ex_error;
    }

    ib_query->trans->affected_rows = 0;

    /* For SELECT statements, mark cursor state as open with rows pending.
     * Check both legacy (out_sqlda) and OO API (fbs_statement) paths. */
    if ((ib_query->statement_type == isc_info_sql_stmt_select ||
         ib_query->statement_type == isc_info_sql_stmt_select_for_upd) &&
        (ib_query->out_sqlda || ib_query->fbs_statement)) {
        ib_query->is_open = 1;
        ib_query->has_more_rows = 1;

        /* OO API SELECT path: return the query resource directly when no SQLDA.
         * The cursor is open via fbs_open_cursor() and fetching will use fbs_fetch(). */
        if (ib_query->fbs_statement && !ib_query->out_sqlda) {
            RETVAL_RES(ib_query->res);
            Z_TRY_ADDREF_P(return_value);
            rv = SUCCESS;
            return rv;
        }
    }

	/* Handle result sets for SELECT, EXECUTE PROCEDURE, and DML with RETURNING clauses */
	if (ib_query->out_sqlda) { /* output variables in select, select for update, or RETURNING */

		/* For EXECUTE PROCEDURE and INSERT/UPDATE/DELETE...RETURNING, create independent result resources to avoid
		 * shared state issues where each execution overwrites the previous result.
		 */
  if (ib_query->statement_type == isc_info_sql_stmt_exec_procedure ||
      ib_query->statement_type == isc_info_sql_stmt_insert ||
      ib_query->statement_type == isc_info_sql_stmt_update ||
      ib_query->statement_type == isc_info_sql_stmt_delete) {
			/* Create a new query structure for this specific result */
			fbird_query *result_query = ecalloc(1, sizeof(fbird_query));

			/* Initialize error cleanup flag */
			int cleanup_needed = 1;

            /* Copy essential fields from the original query */
            result_query->link = ib_query->link;
            result_query->trans = ib_query->trans;
            result_query->trans_res = ib_query->trans_res;
            result_query->dialect = ib_query->dialect;
            result_query->statement_type = ib_query->statement_type;
            result_query->out_fields_count = ib_query->out_fields_count;

            /* OO API snapshot support (EXECUTE PROCEDURE + DML RETURNING)
             *
             * The OO API writes result values into a flat message buffer.
             * For one-shot results, we must snapshot that buffer into the
             * returned result resource so subsequent executions don't
             * overwrite it.
             */
            result_query->fbs_statement = ib_query->fbs_statement;
            result_query->out_metadata = ib_query->out_metadata;
            result_query->out_msg_length = ib_query->out_msg_length;
            result_query->out_msg_buffer = NULL;
            if (ib_query->out_msg_buffer && ib_query->out_msg_length > 0) {
                result_query->out_msg_buffer = safe_emalloc(1, ib_query->out_msg_length, 0);
                memcpy(result_query->out_msg_buffer, ib_query->out_msg_buffer, ib_query->out_msg_length);
            }

            /* Flag used by fbird_fetch_*() to detect buffered RETURNING rows.
             * For EXECUTE PROCEDURE it is ignored, but keep it consistent. */
            result_query->was_result_once = ib_query->was_result_once;

            /* Reuse the original statement handle for metadata operations.
             * This is safe for EXECUTE PROCEDURE and DML RETURNING because
             * there is no open cursor to conflict with, and it enables
             * alias resolution via the newer Firebird API which requires
             * a valid statement handle. */
            result_query->stmt = ib_query->stmt;
            /* Keep a copy of SQL text for symmetry with SELECT path and
             * potential debug/logging uses in helper routines. */
            if (ib_query->query) {
                result_query->query = estrdup(ib_query->query);
            }

			/* Validate source SQLDA before processing */
			if (ib_query->out_sqlda && ib_query->out_fields_count > 0) {
				/* Validate SQLDA structure integrity */
				if (ib_query->out_sqlda->sqln != ib_query->out_fields_count ||
				    ib_query->out_sqlda->sqld != ib_query->out_fields_count) {
					_php_fbird_module_error("EXECUTE PROCEDURE: Invalid SQLDA structure - sqln=%d, sqld=%d, expected=%d",
						ib_query->out_sqlda->sqln, ib_query->out_sqlda->sqld, ib_query->out_fields_count);
					goto cleanup_result_query;
				}

				/* Allocate SQLDA structure with bounds checking */
				size_t sqlda_size = XSQLDA_LENGTH(ib_query->out_fields_count);
				if (sqlda_size < sizeof(XSQLDA) || ib_query->out_fields_count > 32767) {
					_php_fbird_module_error("EXECUTE PROCEDURE: Invalid field count %d for SQLDA allocation",
						ib_query->out_fields_count);
					goto cleanup_result_query;
				}

				result_query->out_sqlda = (XSQLDA *) emalloc(sqlda_size);
				if (!result_query->out_sqlda) {
					_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate SQLDA memory");
					goto cleanup_result_query;
				}

				/* Safe copy of SQLDA header and variable array */
				memcpy(result_query->out_sqlda, ib_query->out_sqlda, sqlda_size);

				/* CRITICAL SAFETY FIX: Clear all sqldata pointers in the copy immediately. */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					result_query->out_sqlda->sqlvar[i].sqldata = NULL;
				}

				/* Allocate null indicator array with validation */
				result_query->out_nullind = safe_emalloc(sizeof(*result_query->out_nullind),
					ib_query->out_fields_count, 0);
				if (!result_query->out_nullind) {
					_php_fbird_module_error("EXECUTE PROCEDURE: Failed to allocate null indicator array");
					goto cleanup_result_query;
				}

				/* Safe copy of null indicators */
				memcpy(result_query->out_nullind, ib_query->out_nullind,
					sizeof(*result_query->out_nullind) * ib_query->out_fields_count);

				/* Deep copy data for each field using safer copying mechanism */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					XSQLVAR *orig_var = &ib_query->out_sqlda->sqlvar[i];
					XSQLVAR *result_var = &result_query->out_sqlda->sqlvar[i];

					/* Reset sqldata pointer - will be set by safe copy function */
					result_var->sqldata = NULL;

					/* Use safer copying function with comprehensive validation */
					if (FAILURE == _php_fbird_safe_copy_sqlvar_data(result_var, orig_var, i, ib_query->query)) {
						goto cleanup_result_query;
					}
				}

				/* Update sqlind pointers to point to the new null indicators */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					if (result_query->out_sqlda->sqlvar[i].sqltype & 1) {
						result_query->out_sqlda->sqlvar[i].sqlind = &result_query->out_nullind[i];
					} else {
						result_query->out_sqlda->sqlvar[i].sqlind = NULL;
					}
				}
			}

   /* Copy input parameter metadata so fbird_num_params()/fbird_param_info()
    * work on the returned result resource (e.g., fbird_query() path). */
   result_query->in_fields_count = ib_query->in_fields_count;
   if (ib_query->in_fields_count > 0 && ib_query->in_sqlda) {
       size_t in_size = XSQLDA_LENGTH(ib_query->in_fields_count);
       result_query->in_sqlda = (XSQLDA *) emalloc(in_size);
       memcpy(result_query->in_sqlda, ib_query->in_sqlda, in_size);
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

            result_query->stmt.ptr = 0; /* Do not reference the handle as it may be freed */

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
			result_query->link = ib_query->link;
			result_query->trans = ib_query->trans;
			result_query->trans_res = ib_query->trans_res;
			result_query->dialect = ib_query->dialect;
			result_query->statement_type = ib_query->statement_type;
			result_query->out_fields_count = ib_query->out_fields_count;
			result_query->was_result_once = 1;

   /* Reuse parent's prepared statement and already-open cursor */
   result_query->stmt = ib_query->stmt;
   result_query->query = estrdup(ib_query->query);

   /* Copy OO API structures for fetch operations
    *
    * IMPORTANT: Snapshot the message buffer.
    * The parent query owns and frees ib_query->out_msg_buffer.
    * Child SELECT results must have their own copy to avoid double-free / UAF
    * when both parent and child resources are destroyed.
    */
   result_query->fbs_statement = ib_query->fbs_statement;
   result_query->out_metadata = ib_query->out_metadata;
   result_query->out_msg_length = ib_query->out_msg_length;
   result_query->out_msg_buffer = NULL;
   if (ib_query->out_msg_buffer && ib_query->out_msg_length > 0) {
       result_query->out_msg_buffer = safe_emalloc(1, ib_query->out_msg_length, 0);
       memcpy(result_query->out_msg_buffer, ib_query->out_msg_buffer, ib_query->out_msg_length);
   }

   /* Copy input parameter metadata so fbird_num_params()/fbird_param_info()
    * work on the returned result resource (e.g., fbird_query() path). */
   result_query->in_fields_count = ib_query->in_fields_count;
   if (ib_query->in_fields_count > 0 && ib_query->in_sqlda) {
       size_t in_size = XSQLDA_LENGTH(ib_query->in_fields_count);
       result_query->in_sqlda = (XSQLDA *) emalloc(in_size);
       memcpy(result_query->in_sqlda, ib_query->in_sqlda, in_size);
       /* Input buffer pointers are not needed on the result copy when
        * reusing the same open cursor; keep them NULL to avoid misuse. */
       for (int i = 0; i < result_query->in_sqlda->sqld; i++) {
           result_query->in_sqlda->sqlvar[i].sqlind = NULL;
           result_query->in_sqlda->sqlvar[i].sqldata = NULL;
       }
   }

   /* Create independent copies of result data structures */
   if (ib_query->out_fields_count > 0 && ib_query->out_sqlda) {
                /* Validate source SQLDA before processing */
                if (ib_query->out_sqlda->sqln != ib_query->out_fields_count ||
                    ib_query->out_sqlda->sqld != ib_query->out_fields_count) {
                    _php_fbird_module_error("SELECT: Invalid SQLDA structure - sqln=%d, sqld=%d, expected=%d",
                        ib_query->out_sqlda->sqln, ib_query->out_sqlda->sqld, ib_query->out_fields_count);
                    goto cleanup_select_result_query;
                }

				/* Allocate independent SQLDA structure */
				size_t sqlda_size = XSQLDA_LENGTH(ib_query->out_fields_count);
				result_query->out_sqlda = (XSQLDA *) emalloc(sqlda_size);
				if (!result_query->out_sqlda) {
					_php_fbird_module_error("SELECT: Failed to allocate SQLDA memory");
					goto cleanup_select_result_query;
				}

				/* Safe copy of SQLDA header and variable array */
				memcpy(result_query->out_sqlda, ib_query->out_sqlda, sqlda_size);

				/* CRITICAL SAFETY FIX: Clear all sqldata pointers immediately to prevent
				 * double-free of parent data if allocation loop fails. */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					result_query->out_sqlda->sqlvar[i].sqldata = NULL;
				}

				/* Allocate independent null indicator array */
				result_query->out_nullind = safe_emalloc(sizeof(*result_query->out_nullind),
					ib_query->out_fields_count, 0);
				if (!result_query->out_nullind) {
					_php_fbird_module_error("SELECT: Failed to allocate null indicator array");
					goto cleanup_select_result_query;
				}

				/* Safe copy of null indicators */
				memcpy(result_query->out_nullind, ib_query->out_nullind,
					sizeof(*result_query->out_nullind) * ib_query->out_fields_count);

				/* Copy row buffers only when legacy path populated sqldata.
				 * In OO API mode, out_sqlda is metadata-only and fetch uses out_msg_buffer.
				 */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					XSQLVAR *orig_var = &ib_query->out_sqlda->sqlvar[i];
					XSQLVAR *result_var = &result_query->out_sqlda->sqlvar[i];

					result_var->sqldata = NULL;

					if (orig_var->sqldata) {
						if (FAILURE == _php_fbird_safe_copy_sqlvar_data(result_var, orig_var, i, ib_query->query)) {
							goto cleanup_select_result_query;
						}
					}
				}

				/* Update sqlind pointers to point to the new null indicators */
				for (int i = 0; i < ib_query->out_fields_count; i++) {
					if (result_query->out_sqlda->sqlvar[i].sqltype & 1) {
						result_query->out_sqlda->sqlvar[i].sqlind = &result_query->out_nullind[i];
					} else {
						result_query->out_sqlda->sqlvar[i].sqlind = NULL;
					}
				}

				/* Copy array metadata if present */
				if (ib_query->out_array_cnt > 0 && ib_query->out_array) {
					result_query->out_array_cnt = ib_query->out_array_cnt;
					result_query->out_array = safe_emalloc(sizeof(fbird_array), ib_query->out_array_cnt, 0);
					if (!result_query->out_array) {
						_php_fbird_module_error("SELECT: Failed to allocate array metadata");
						goto cleanup_select_result_query;
					}
					memcpy(result_query->out_array, ib_query->out_array,
						sizeof(fbird_array) * ib_query->out_array_cnt);
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
            result_query->parent = ib_query;
            result_query->child_head = NULL;
            result_query->child_next = ib_query->child_head;
            ib_query->child_head = result_query;

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
	switch (ib_query->statement_type) {

		unsigned long affected_rows;

		case isc_info_sql_stmt_insert:
		case isc_info_sql_stmt_update:
		case isc_info_sql_stmt_delete:
		case isc_info_sql_stmt_exec_procedure:

			affected_rows = 0;

			/* OO API Only: compute affected rows via statement wrapper. */
			if (ib_query->fbs_statement) {
				ISC_UINT64 oo_affected = fbs_get_affected_records(
					IBG(master_instance),
					ib_query->fbs_statement,
					IB_STATUS
				);

				if (IB_STATUS[0] == 1 && IB_STATUS[1] != 0) {
					_php_fbird_error();
					goto _php_fbird_ex_error;
				}

				affected_rows = (unsigned long)oo_affected;
			} else {
				/* No OO API statement - cannot get affected rows */
				_php_fbird_module_error("Cannot get affected rows: OO API statement required");
				goto _php_fbird_ex_error;
			}

			ib_query->trans->affected_rows = affected_rows;

			if (!ib_query->out_sqlda) { /* no result set is being returned */
				/* Non-SELECT statements without RETURNING clause - no cursor opened */
				ib_query->is_open = 0;
				ib_query->has_more_rows = 0;

				if (affected_rows) {
					RETVAL_LONG(affected_rows);
				} else {
					RETVAL_TRUE;
				}
				break;
			}

			/* DML with RETURNING clause - cursor is opened but handled by result resource */
			ib_query->is_open = 0;
			ib_query->has_more_rows = 0;
			break;

		case isc_info_sql_stmt_select:
		case isc_info_sql_stmt_select_for_upd:
			/* SELECT statements (including SELECT ... FOR UPDATE) - cursor is now open and has potential rows.
			 * Check both legacy (out_sqlda) and OO API (fbs_statement) paths. */
			if (ib_query->out_sqlda || ib_query->fbs_statement) {
				ib_query->is_open = 1;
				ib_query->has_more_rows = 1;
			} else {
				/* SELECT without output - unusual but handle */
				ib_query->is_open = 0;
				ib_query->has_more_rows = 0;
			}
			break;

		default:
			/* Other statement types (DDL, etc.) - no cursor */
			ib_query->is_open = 0;
			ib_query->has_more_rows = 0;
			RETVAL_TRUE;
			break;
	}

	rv = SUCCESS;

_php_fbird_ex_error:
	/* Only clear cursor flags on actual execution error, not on success.
	 * The OO API path sets these flags correctly before reaching here. */
	if (rv == FAILURE) {
		ib_query->is_open = 0;
		ib_query->has_more_rows = 0;
	}
	return rv;
}
/* }}} */

/* {{{ proto mixed fbird_query([resource link_identifier, [ resource link_identifier, ]] string query [, mixed bind_arg [, mixed bind_arg [, ...]]]) */
PHP_FUNCTION(fbird_query)
{
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	char *query = NULL;
	fbird_db_link *link = NULL;
	fbird_transaction *trans = NULL;
	zval *link_arg = NULL, *trans_arg = NULL;
	zend_resource *trans_res = NULL;
	fbird_query *ib_query;
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
			explicit_create = 1;
			i++;
			continue;
		}

		if (Z_TYPE_P(arg) == IS_STRING) {
			query = Z_STRVAL_P(arg);
			bind_start = i + 1;
			break;
		} else if (Z_TYPE_P(arg) == IS_RESOURCE) {
			/* Identify resource type */
			if (!trans && !link) {
				trans = (fbird_transaction *)zend_fetch_resource_ex(arg, NULL, le_trans);
				if (trans) {
					trans_arg = arg;
					trans_res = Z_RES_P(trans_arg);
				} else {
					link = (fbird_db_link *)zend_fetch_resource_ex(arg, NULL, le_link);
					if (!link) link = (fbird_db_link *)zend_fetch_resource_ex(arg, NULL, le_plink);
					if (link) link_arg = arg;
				}
			} else if (trans && !link) {
				link = (fbird_db_link *)zend_fetch_resource_ex(arg, NULL, le_link);
				if (!link) link = (fbird_db_link *)zend_fetch_resource_ex(arg, NULL, le_plink);
				if (link) link_arg = arg;
			} else if (link && !trans) {
				trans = (fbird_transaction *)zend_fetch_resource_ex(arg, NULL, le_trans);
				if (trans) {
					trans_arg = arg;
					trans_res = Z_RES_P(trans_arg);
				}
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
			IBG(master_instance),
			query,
			dialect,
			IB_STATUS
		);

		if (!create_result) {
			_php_fbird_error();
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
		link->handle.ptr = NULL; /* No legacy handle for OO API connection */

		/* Store the OO API connection wrapper.
		 * The create_result is a pointer that fbc_get_attachment() can use. */
		link->fbc_connection = create_result;

		RETVAL_RES(zend_register_resource(link, le_link));
		efree(args);
		return;
	}

	/* Resolve Link if missing */
	if (!link && !trans) {
		if (IBG(default_link)) {
			link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), "Firebird link", le_link, le_plink);
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

	if (FAILURE == _php_fbird_prepare(&ib_query, link, trans, trans_res, query)) {
		efree(args);
		RETURN_FALSE;
	}

	for (i = bind_start; i < argc; i++) {
		Z_TRY_ADDREF(args[i]);
	}

	if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, &args[bind_start], argc - bind_start)) {
		zend_list_delete(ib_query->res);
		for (i = bind_start; i < argc; i++) {
			zval_ptr_dtor(&args[i]);
		}
		efree(args);
		RETURN_FALSE;
	}

	if (Z_TYPE_P(return_value) != IS_RESOURCE) {
	    zend_list_delete(ib_query->res);
	}

	for (i = bind_start; i < argc; i++) {
		zval_ptr_dtor(&args[i]);
	}
	efree(args);
}
/* }}} */

/* {{{ proto resource fbird_prepare([resource link_identifier, [ resource link_identifier, ]] string query) */
PHP_FUNCTION(fbird_prepare)
{
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	char *query = NULL;
	fbird_db_link *link = NULL;
	fbird_transaction *trans = NULL;
	zval *link_arg = NULL, *trans_arg = NULL;
	zend_resource *trans_res = NULL;
	fbird_query *ib_query;

	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	/* Parse arguments */
	i = 0;
	if (Z_TYPE(args[i]) == IS_RESOURCE) {
		trans = (fbird_transaction *)zend_fetch_resource_ex(&args[i], NULL, le_trans);
		if (trans) {
			trans_arg = &args[i];
			trans_res = Z_RES_P(trans_arg);
			i++;
		} else {
			link = (fbird_db_link *)zend_fetch_resource_ex(&args[i], NULL, le_link);
			if (!link) {
				link = (fbird_db_link *)zend_fetch_resource_ex(&args[i], NULL, le_plink);
			}
			if (link) {
				link_arg = &args[i];
				i++;
			}
		}
	}

	if (i == 1 && i < argc && Z_TYPE(args[i]) == IS_RESOURCE) {
		if (trans) {
			fbird_db_link *l = (fbird_db_link *)zend_fetch_resource_ex(&args[i], NULL, le_link);
			if (!l) l = (fbird_db_link *)zend_fetch_resource_ex(&args[i], NULL, le_plink);
			if (l) {
				link = l;
				link_arg = &args[i];
				i++;
			}
		} else if (link) {
			fbird_transaction *t = (fbird_transaction *)zend_fetch_resource_ex(&args[i], NULL, le_trans);
			if (t) {
				trans = t;
				trans_arg = &args[i];
				trans_res = Z_RES_P(trans_arg);
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
		if (IBG(default_link)) {
			link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), "Firebird link", le_link, le_plink);
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
	if (FAILURE == _php_fbird_prepare(&ib_query, link, trans, trans_res, query)) {
		efree(args);
		RETURN_FALSE;
	}

	efree(args);
	RETVAL_RES(ib_query->res);
	Z_TRY_ADDREF_P(return_value);
}
/* }}} */

/* {{{ proto mixed fbird_execute(resource query [, mixed bind_arg [, mixed bind_arg [, ...]]]) */
PHP_FUNCTION(fbird_execute)
{
	zval *args;
	int i, argc = ZEND_NUM_ARGS();
	fbird_query *ib_query;

	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	ib_query = (fbird_query *)zend_fetch_resource_ex(&args[0], "Firebird query", le_query);
	if (!ib_query) {
		efree(args);
		RETURN_FALSE;
	}

	for (i = 1; i < argc; i++) {
		Z_TRY_ADDREF(args[i]);
	}

	if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, &args[1], argc - 1)) {
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
}
/* }}} */

/* {{{ proto bool fbird_free_query(resource query) */
void _php_fbird_free_query_impl(INTERNAL_FUNCTION_PARAMETERS, int as_result)
{
	zval *query_arg;
	fbird_query *ib_query;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &query_arg) == FAILURE) {
		return;
	}

	ib_query = (fbird_query *)zend_fetch_resource_ex(query_arg, "Firebird query", le_query);
	if (!ib_query) {
		RETURN_FALSE;
	}

	zend_list_close(Z_RES_P(query_arg));
	RETURN_TRUE;
}

PHP_FUNCTION(fbird_free_query)
{
	_php_fbird_free_query_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto int fbird_affected_rows([ resource link_identifier ]) */
PHP_FUNCTION(fbird_affected_rows)
{
	zval *link_arg = NULL;
	fbird_db_link *link = NULL;
	fbird_transaction *trans = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &link_arg) == FAILURE) {
		return;
	}

	if (link_arg) {
		link = (fbird_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
	} else {
		if (IBG(default_link)) {
			link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), "Firebird link", le_link, le_plink);
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
/* }}} */

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

/* {{{ proto int fbird_execute_statement(resource trans_handle, string query [, array params])
   Execute DML/DDL statement within a specific transaction and return affected rows */
PHP_FUNCTION(fbird_execute_statement)
{
    zval *trans_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    fbird_transaction *trans;
    fbird_db_link *link = NULL;
    fbird_query *ib_query;
    zval *bind_args = NULL;
    int bind_n = 0;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs|a", &trans_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    trans = (fbird_transaction *)zend_fetch_resource_ex(trans_arg, LE_TRANS, le_trans);
    if (!trans) {
        RETURN_FALSE;
    }
    if (trans->link_cnt > 0) {
        link = trans->db_link[0];
    } else {
        _php_fbird_module_error("Transaction has no associated link");
        RETURN_FALSE;
    }

    if (FAILURE == _php_fbird_prepare(&ib_query, link, trans, Z_RES_P(trans_arg), sql)) {
        RETURN_FALSE;
    }

    if (params_arg) {
        bind_args = _php_fbird_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }
        zend_list_delete(ib_query->res);
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
        zend_list_delete(ib_query->res);
        RETURN_THROWS();
    }

    /* Cleanup prepared query resource as fbird_execute_statement is one-shot for the user?
       Wait, fbird_execute_statement takes SQL + Params. It prepares, executes, then destroys query handle?
       Yes, similar to fbird_query execution path.
       If the user wants prepared statement reuse, they should use fbird_prepare + fbird_execute.
       fbird_execute_statement is atomic execution.
    */
    zend_list_delete(ib_query->res);

    if (Z_TYPE_P(return_value) == IS_TRUE) {
        RETVAL_LONG(0);
    }
}
/* }}} */

/* {{{ proto resource fbird_execute_query(resource trans_handle, string query [, array params])
   Execute SELECT statement within a specific transaction and return result resource */
PHP_FUNCTION(fbird_execute_query)
{
    zval *trans_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    fbird_transaction *trans;
    fbird_db_link *link = NULL;
    fbird_query *ib_query;
    zval *bind_args = NULL;
    int bind_n = 0;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs|a", &trans_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    trans = (fbird_transaction *)zend_fetch_resource_ex(trans_arg, LE_TRANS, le_trans);
    if (!trans) RETURN_FALSE;
    if (trans->link_cnt > 0) {
        link = trans->db_link[0];
    } else {
        _php_fbird_module_error("Transaction has no associated link");
        RETURN_FALSE;
    }

    /* cppcheck-suppress legacyUninitvar ; link is guaranteed non-NULL here */
    if (FAILURE == _php_fbird_prepare(&ib_query, link, trans, Z_RES_P(trans_arg), sql)) {
        RETURN_FALSE;
    }

    if (params_arg) {
        bind_args = _php_fbird_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }
        zend_list_delete(ib_query->res);
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
        zend_list_delete(ib_query->res);
        RETURN_THROWS();
    }

    /* Keep ib_query alive as it holds the statement handle */
    /* But wait, _php_fbird_exec creates a RESULT resource that references ib_query.
       If ib_query is just for execution, we should probably keep it alive managed by the result.
       Actually _php_fbird_exec implementation for SELECT reuses ib_query->stmt.
       And it sets result_query->parent = ib_query.
       So we MUST return the result resource (which is in return_value)
       AND let ib_query be managed.
       Actually, fbird_query implementation returns the result resource but keeps ib_query resource alive?
       Wait, fbird_query deletes ib_query->res ONLY on error.
       So on success, ib_query->res is alive.
       Is it returned? No, return_value is the result_query->res.
       So ib_query (the prepared statement) leaks?
       No, fbird_query is one-shot.
       Let's check fbird_query implementation again.
       It does zend_list_delete(ib_query->res) ONLY on error label.
       If success, it returns.
       So ib_query resource leaks?
       Ah, for SELECT, _php_fbird_exec returns result_query->res.
       result_query->parent = ib_query.
       So ib_query resource must persist for the lifetime of result?
       Yes.
       So we DO NOT delete ib_query->res on success.
    */
}
/* }}} */

/* {{{ proto mixed fbird_execute_auto(resource link_identifier, string query [, array params])
   Execute statement in an autonomous transaction (start -> execute -> commit/rollback) */
PHP_FUNCTION(fbird_execute_auto)
{
    zval *link_arg, *params_arg = NULL;
    char *sql;
    size_t sql_len;
    fbird_db_link *link;
    fbird_transaction *trans;
    fbird_query *ib_query;
    zval *bind_args = NULL;
    int bind_n = 0;
    void *oo_trans = NULL;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs|a", &link_arg, &sql, &sql_len, &params_arg) == FAILURE) {
        return;
    }

    link = (fbird_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
    if (!link) RETURN_FALSE;

    /* OO API Only: Connection must have OO API handle */
    if (!link->fbc_connection) {
        _php_fbird_module_error("fbird_execute_auto requires OO API connection (fbc_connection required)");
        RETURN_FALSE;
    }

    /* Start autonomous transaction via OO API */
    void *attachment = fbc_get_attachment(link->fbc_connection);
    oo_trans = fbt_start(IBG(master_instance), attachment, 0, NULL, IB_STATUS);
    if (!oo_trans) {
        _php_fbird_error();
        RETURN_FALSE;
    }

    /* Create temp trans object with OO API transaction */
    trans = (fbird_transaction *) emalloc(sizeof(fbird_transaction));
    trans->handle.ptr = NULL; /* No legacy handle for OO API transaction */
    trans->link_cnt = 1;
    trans->affected_rows = 0;
    trans->fbt_transaction = oo_trans;
    trans->db_link[0] = link;
    /* We do NOT register this transaction as a resource because it's strictly local scope */

    /* Prepare */
    if (FAILURE == _php_fbird_prepare(&ib_query, link, trans, NULL, sql)) {
        fbt_rollback(oo_trans, IB_STATUS);
        efree(trans);
        RETURN_FALSE;
    }

    if (params_arg) {
        bind_args = _php_fbird_hash_to_zval_array(Z_ARRVAL_P(params_arg), &bind_n);
    }

    if (FAILURE == _php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU, ib_query, bind_args, bind_n)) {
        if (bind_args) {
            for (int i=0; i<bind_n; i++) zval_ptr_dtor(&bind_args[i]);
            efree(bind_args);
        }

        zend_list_delete(ib_query->res); // Frees statement
        fbt_rollback(oo_trans, IB_STATUS);
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
        zend_list_delete(ib_query->res);
        fbt_rollback(oo_trans, IB_STATUS);
        efree(trans);
        RETURN_THROWS();
    }

    /* Free statement BEFORE committing transaction to avoid
     * fbs_free() issues when the transaction is already gone.
     * Save the result value before deleting the query resource. */
    zval saved_result;
    ZVAL_COPY_VALUE(&saved_result, return_value);
    zend_list_delete(ib_query->res);

    /* Commit via OO API (returns 0 on success, non-zero on error) */
    if (fbt_commit(oo_trans, IB_STATUS)) {
        _php_fbird_error();
        efree(trans);
        RETURN_FALSE;
    }

    efree(trans);

    /* Restore return value (affected rows count) */
    ZVAL_COPY_VALUE(return_value, &saved_result);

    /* Return value is already set by _php_fbird_exec (TRUE/affected_rows) */
}
/* }}} */

int _php_fbird_fetch_query_res(zval *from, fbird_query **ib_query)
{
	if (Z_TYPE_P(from) != IS_RESOURCE) {
		return 0;
	}
	*ib_query = (fbird_query *)zend_fetch_resource_ex(from, "Firebird query", le_query);
	return (*ib_query) ? 1 : 0;
}

#endif /* HAVE_FIREBIRD */

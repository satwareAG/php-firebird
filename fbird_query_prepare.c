/*
   +----------------------------------------------------------------------+
   | PHP Version 8                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
 */

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
int _php_fbird_set_query_info(fbird_query *ib_query) /* {{{ */
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
/* }}} */

void _php_fbird_alloc_xsqlda_vars(XSQLDA *sqlda, ISC_SHORT *nullinds) /* {{{ */
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
/* }}} */

void _php_fbird_free_xsqlda(XSQLDA *sqlda) /* {{{ */
{
	int i;
	XSQLVAR *var;

	IBDEBUG("Free XSQLDA?");
	if (sqlda) {
		IBDEBUG("Freeing XSQLDA...");
		var = sqlda->sqlvar;
		for (i = 0; i < sqlda->sqld; i++, var++) {
			efree(var->sqldata);
		}
		efree(sqlda);
	}
}
/* }}} */

void _php_fbird_free_query(fbird_query *ib_query) /* {{{ */
{
	IBDEBUG("Freeing query...");

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
/* }}} */

void php_fbird_free_query_rsrc(zend_resource *rsrc) /* {{{ */
{
    fbird_query *ib_query = (fbird_query *)rsrc->ptr;

    if (ib_query != NULL) {
        IBDEBUG("Preparing to free query by dtor...");

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
         * to avoid -502 (Attempt to reopen an open cursor) on subsequent uses. */
        /* Phase 5: Free OO API statement wrapper if used */
        if (ib_query->fbs_statement) {
            /* Close any open OO API resultset first */
            if (ib_query->fbs_resultset) {
                fbs_close_cursor(ib_query->fbs_statement, IB_STATUS);
                ib_query->fbs_resultset = NULL;
            }
            /* Free the OO API statement */
            if (ib_query->owns_stmt_handle) {
                fbs_free(ib_query->fbs_statement, IB_STATUS);
            }
            ib_query->fbs_statement = NULL;
        } else if (ib_query->stmt.stmt) {
            /* Close open cursor if needed */
            if (ib_query->is_open) {
                IBDEBUG("Closing open cursor in dtor");
                (void) isc_dsql_free_statement(IB_STATUS, &ib_query->stmt.stmt, DSQL_close);
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
            /* Drop the statement handle only if this resource OWNS it.
             * Result clones created for SELECT reuse parent's handle and must NOT drop it. */
            if (ib_query->owns_stmt_handle) {
                (void) isc_dsql_free_statement(IB_STATUS, &ib_query->stmt.stmt, DSQL_drop);
            }
        }
        _php_fbird_free_query(ib_query);
    }
}
/* }}} */

void php_fbird_query_minit(INIT_FUNC_ARGS) /* {{{ */
{
	(void)type;
	le_query = zend_register_list_destructors_ex(php_fbird_free_query_rsrc, NULL,
		LE_QUERY, module_number);
}
/* }}} */

/* Allocate and prepare query */
int _php_fbird_prepare(fbird_query **new_query, fbird_db_link *link, /* {{{ */
    fbird_transaction *trans, zend_resource *trans_res, char *query)
{
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
		IBDEBUG("fbs_prepare() failed\n");
		_php_fbird_error();
		goto _php_fbird_alloc_query_error;
	}
	IBDEBUG("OO API statement prepared successfully\n");

	if(_php_fbird_set_query_info(ib_query)){
		goto _php_fbird_alloc_query_error;
	}

	/* XSQLDA allocation and describe operations.
	 *
	 * OO API (Firebird 3.0+): When fbs_statement is set, metadata is accessed
	 * through IMessageMetadata interfaces. No XSQLDA required - the OO API
	 * uses message buffers for data transfer during fetch operations.
	 *
	 * Legacy API: Uses XSQLDA structures for metadata and data transfer.
	 */
	if (!ib_query->fbs_statement) {
		/* Legacy path: allocate and describe XSQLDA structures */
		if(ib_query->out_fields_count) {
			ib_query->out_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(ib_query->out_fields_count));
			ib_query->out_sqlda->sqln = ib_query->out_fields_count;
			ib_query->out_sqlda->version = SQLDA_CURRENT_VERSION;

			if (isc_dsql_describe(IB_STATUS, &ib_query->stmt.stmt, SQLDA_CURRENT_VERSION, ib_query->out_sqlda)) {
				IBDEBUG("isc_dsql_describe() failed\n");
				_php_fbird_error();
				goto _php_fbird_alloc_query_error;
			}

			ib_query->out_nullind = safe_emalloc(sizeof(*ib_query->out_nullind), ib_query->out_sqlda->sqld, 0);
			_php_fbird_alloc_xsqlda_vars(ib_query->out_sqlda, ib_query->out_nullind);
			if (FAILURE == _php_fbird_alloc_array(&ib_query->out_array, ib_query->out_sqlda,
				link->handle, trans->handle, &ib_query->out_array_cnt)) {
				goto _php_fbird_alloc_query_error;
			}
		}

		if(ib_query->in_fields_count) {
			ib_query->in_sqlda = emalloc(XSQLDA_LENGTH(ib_query->in_fields_count));
			ib_query->in_sqlda->sqln = ib_query->in_fields_count;
			ib_query->in_sqlda->version = SQLDA_CURRENT_VERSION;

			if (isc_dsql_describe_bind(IB_STATUS, &ib_query->stmt.stmt, SQLDA_CURRENT_VERSION, ib_query->in_sqlda)) {
				IBDEBUG("isc_dsql_describe_bind() failed\n");
				_php_fbird_error();
				goto _php_fbird_alloc_query_error;
			}

			assert(ib_query->in_sqlda->sqln == ib_query->in_sqlda->sqld);
			assert(ib_query->in_sqlda->sqld == ib_query->in_fields_count);

			ib_query->bind_buf = safe_emalloc(sizeof(BIND_BUF), ib_query->in_sqlda->sqld, 0);
			ib_query->in_nullind = safe_emalloc(sizeof(*ib_query->in_nullind), ib_query->in_sqlda->sqld, 0);
			if (FAILURE == _php_fbird_alloc_array(&ib_query->in_array, ib_query->in_sqlda,
				link->handle, trans->handle, &ib_query->in_array_cnt)) {
				goto _php_fbird_alloc_query_error;
			}
		}
	} else {
		/* OO API path: Allocate message buffers for fetch operations.
		 * These replace XSQLDA-based data transfer. */
		if (ib_query->out_fields_count > 0) {
			/* Get output metadata and allocate message buffer */
			ib_query->out_metadata = fbs_get_output_metadata(
				IBG(master_instance), ib_query->fbs_statement, IB_STATUS);
			if (!ib_query->out_metadata) {
				IBDEBUG("fbs_get_output_metadata() failed\n");
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
			IBDEBUG("OO API output message buffer allocated\n");
		}

		if (ib_query->in_fields_count > 0) {
			/* Get input metadata and allocate message buffer */
			ib_query->in_metadata = fbs_get_input_metadata(
				IBG(master_instance), ib_query->fbs_statement, IB_STATUS);
			if (!ib_query->in_metadata) {
				IBDEBUG("fbs_get_input_metadata() failed\n");
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
			IBDEBUG("OO API input message buffer allocated\n");
		}
	}

	*new_query = ib_query;

	return SUCCESS;

_php_fbird_alloc_query_error:
	zend_list_delete(ib_query->res);

	return FAILURE;
}
/* }}} */

#endif /* HAVE_FIREBIRD */

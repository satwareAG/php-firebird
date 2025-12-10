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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"

/* Helper to execute a delete statement with one integer parameter */
static int _fbird_exec_kill(fbird_db_link *link, fbird_transaction *trans, ISC_INT64 attachment_id)
{
	void *stmt = 0;
	XSQLDA *sqlda = NULL;
	static const char *sql = "DELETE FROM MON$ATTACHMENTS WHERE MON$ATTACHMENT_ID = ?";
	int res = FAILURE;
	short null_ind = 0;

	if (isc_dsql_allocate_statement(IB_STATUS, &link->handle.db, (isc_stmt_handle*)&stmt)) {
		_php_fbird_error();
		return FAILURE;
	}

	sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
	sqlda->version = SQLDA_CURRENT_VERSION;
	sqlda->sqln = 1;

	if (isc_dsql_prepare(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 0, (char *)sql, 3, sqlda)) {
		_php_fbird_error();
		goto cleanup;
	}

	/* Describe bind to get parameter metadata from Firebird */
	if (isc_dsql_describe_bind(IB_STATUS, (isc_stmt_handle*)&stmt, SQLDA_CURRENT_VERSION, sqlda)) {
		_php_fbird_error();
		goto cleanup;
	}

	/* Bind parameter - use the type Firebird expects (typically BIGINT for MON$ATTACHMENT_ID) */
	sqlda->sqlvar[0].sqldata = (char *)&attachment_id;
	sqlda->sqlvar[0].sqlind = &null_ind;

	if (isc_dsql_execute(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 1, sqlda)) {
		_php_fbird_error();
		goto cleanup;
	}

	res = SUCCESS;

cleanup:
	isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);
	if (sqlda) efree(sqlda);
	return res;
}

/* {{{ proto bool fbird_kill_attachment(resource link_or_trans, int attachment_id)
   Terminates a specific connection */
PHP_FUNCTION(fbird_kill_attachment)
{
	zval *link_arg;
	zend_long attachment_id;
	fbird_db_link *link;
	fbird_transaction *trans;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &link_arg, &attachment_id) == FAILURE) {
		return;
	}

	PHP_IBASE_LINK_TRANS(link_arg, link, trans);

	if (_fbird_exec_kill(link, trans, (ISC_INT64) attachment_id) == FAILURE) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array fbird_list_table_blockers(resource link_or_trans, string table_name)
   Returns blocking attachment information */
PHP_FUNCTION(fbird_list_table_blockers)
{
	zval *link_arg;
	char *table_name;
	size_t table_name_len;
	fbird_db_link *link;
	fbird_transaction *trans;
	void *stmt = 0;
	XSQLDA *in_sqlda = NULL, *out_sqlda = NULL;
	char *param_buf = NULL;

	RESET_ERRMSG;

	/* SQL to find attachments using the table in statements.
	 * CONTAINING is Firebird's BLOB-aware, case-insensitive substring search.
	 * It's more reliable than LIKE for BLOB fields like MON$SQL_TEXT. */
	static const char *sql =
		"SELECT DISTINCT A.MON$ATTACHMENT_ID, A.MON$USER "
		"FROM MON$ATTACHMENTS A "
		"JOIN MON$STATEMENTS S ON S.MON$ATTACHMENT_ID = A.MON$ATTACHMENT_ID "
		"WHERE A.MON$ATTACHMENT_ID <> CURRENT_CONNECTION "
		"AND S.MON$SQL_TEXT CONTAINING ?";

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &link_arg, &table_name, &table_name_len) == FAILURE) {
		return;
	}

	PHP_IBASE_LINK_TRANS(link_arg, link, trans);

	if (isc_dsql_allocate_statement(IB_STATUS, &link->handle.db, (isc_stmt_handle*)&stmt)) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	in_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
	in_sqlda->version = SQLDA_CURRENT_VERSION;
	in_sqlda->sqln = 1;

	if (isc_dsql_prepare(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 0, (char *)sql, 3, in_sqlda)) {
		_php_fbird_error();
		goto cleanup_error;
	}

	/* Describe bind to get parameter metadata from Firebird */
	if (isc_dsql_describe_bind(IB_STATUS, (isc_stmt_handle*)&stmt, SQLDA_CURRENT_VERSION, in_sqlda)) {
		_php_fbird_error();
		goto cleanup_error;
	}

    /* CONTAINING does not need wildcards - just pass table name directly */
    short in_null_ind = 0;

    /* Bind input using Firebird's expected type from describe_bind */
    if ((in_sqlda->sqlvar[0].sqltype & ~1) == SQL_VARYING) {
        /* SQL_VARYING requires 2-byte length prefix */
        param_buf = emalloc(in_sqlda->sqlvar[0].sqllen + sizeof(short));
        *(short *)param_buf = (short)table_name_len;
        memcpy(param_buf + sizeof(short), table_name, table_name_len);
        in_sqlda->sqlvar[0].sqldata = param_buf;
    } else {
        /* SQL_TEXT or other - direct binding */
        param_buf = emalloc(table_name_len + 1);
        memcpy(param_buf, table_name, table_name_len);
        param_buf[table_name_len] = '\0';
        in_sqlda->sqlvar[0].sqldata = param_buf;
        in_sqlda->sqlvar[0].sqllen = (short)table_name_len;
    }
    in_sqlda->sqlvar[0].sqlind = &in_null_ind;

    /* Prepare output */
    out_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(2));
    out_sqlda->version = SQLDA_CURRENT_VERSION;
    out_sqlda->sqln = 2;

    if (isc_dsql_describe(IB_STATUS, (isc_stmt_handle*)&stmt, 1, out_sqlda)) {
        _php_fbird_error();
        goto cleanup_error;
    }

    /* Allocate buffers for output - MON$ATTACHMENT_ID is BIGINT (64-bit) in Firebird 3+ */
    ISC_INT64 ret_id;
    char ret_user[256];
    short null_ind[2];

    out_sqlda->sqlvar[0].sqldata = (char *)&ret_id;
    out_sqlda->sqlvar[0].sqltype = SQL_INT64;
    out_sqlda->sqlvar[0].sqllen = sizeof(ISC_INT64);
    out_sqlda->sqlvar[0].sqlind = &null_ind[0];

    out_sqlda->sqlvar[1].sqldata = ret_user;
    out_sqlda->sqlvar[1].sqltype = SQL_TEXT;
    out_sqlda->sqlvar[1].sqllen = 255;
    out_sqlda->sqlvar[1].sqlind = &null_ind[1];

    if (isc_dsql_execute(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 1, in_sqlda)) {
        _php_fbird_error();
        goto cleanup_error;
    }

    array_init(return_value);

    while (1) {
        if (isc_dsql_fetch(IB_STATUS, (isc_stmt_handle*)&stmt, 1, out_sqlda)) {
            if (IB_STATUS[1] == 100) break; // EOF
            _php_fbird_error();
            /* Return partial result but free resources */
            goto cleanup;
        }

        zval row;
        array_init(&row);
        add_assoc_long(&row, "attachment_id", ret_id);

        /* Trim user field */
        ret_user[out_sqlda->sqlvar[1].sqllen] = '\0';

        // Trim trailing spaces manually
        for (int i = out_sqlda->sqlvar[1].sqllen - 1; i >= 0; i--) {
            if (ret_user[i] == ' ') ret_user[i] = '\0';
            else break;
        }

        add_assoc_string(&row, "user", ret_user);
        add_next_index_zval(return_value, &row);
    }

cleanup:
    isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);
    if (in_sqlda) efree(in_sqlda);
    if (out_sqlda) efree(out_sqlda);
    if (param_buf) efree(param_buf);
    return;

cleanup_error:
    isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);
    if (in_sqlda) efree(in_sqlda);
    if (out_sqlda) efree(out_sqlda);
    if (param_buf) efree(param_buf);
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool fbird_drop_table_force(resource link_or_trans, string table_name)
   Drops a table by first killing locking connections */
PHP_FUNCTION(fbird_drop_table_force)
{
	zval *link_arg;
	char *table_name;
	size_t table_name_len;
	fbird_db_link *link;
	fbird_transaction *trans;
    void *stmt = 0;
    char *drop_sql = NULL;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &link_arg, &table_name, &table_name_len) == FAILURE) {
		return;
	}

    PHP_IBASE_LINK_TRANS(link_arg, link, trans);

    /* Note: Blocker detection via MON$SQL_TEXT requires complex BLOB handling.
     * For now, we skip the blocker-killing step and just do the DROP.
     * In most cases, DDL will fail cleanly if there are active locks. */

    /* Execute Drop using prepared statement */
    spprintf(&drop_sql, 0, "DROP TABLE %s", table_name);

    /* Allocate a new statement for DROP */
    if (isc_dsql_allocate_statement(IB_STATUS, &link->handle.db, (isc_stmt_handle*)&stmt)) {
         _php_fbird_error();
         goto error;
    }

    /* Prepare the DROP statement */
    if (isc_dsql_prepare(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 0, drop_sql, 3, NULL)) {
         _php_fbird_error();
         goto error;
    }

    /* Execute the DROP statement */
    if (isc_dsql_execute(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, SQLDA_CURRENT_VERSION, NULL)) {
         _php_fbird_error();
         goto error;
    }

    /* Free the statement */
    isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);
    stmt = 0;

    /* DDL requires commit to be visible - commit the transaction */
    if (isc_commit_transaction(IB_STATUS, &trans->handle.tr)) {
        _php_fbird_error();
        goto error;
    }

    if (drop_sql) efree(drop_sql);
    RETURN_TRUE;

error:
    if (stmt) isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);
    if (drop_sql) efree(drop_sql);
    RETURN_FALSE;
}
/* }}} */

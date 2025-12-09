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
static int _fbird_exec_kill(ibase_db_link *link, ibase_trans *trans, ISC_LONG attachment_id)
{
	void *stmt = 0;
	XSQLDA *sqlda = NULL;
	static const char *sql = "DELETE FROM MON$ATTACHMENTS WHERE MON$ATTACHMENT_ID = ?";
	int res = FAILURE;

	if (isc_dsql_allocate_statement(IB_STATUS, &link->handle.db, (isc_stmt_handle*)&stmt)) {
		_php_ibase_error();
		return FAILURE;
	}

	sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
	sqlda->version = SQLDA_CURRENT_VERSION;
	sqlda->sqln = 1;

	if (isc_dsql_prepare(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 0, (char *)sql, 1, sqlda)) {
		_php_ibase_error();
		goto cleanup;
	}

	/* Bind parameter */
	sqlda->sqlvar[0].sqldata = (char *)&attachment_id;
	sqlda->sqlvar[0].sqltype = SQL_LONG;
	sqlda->sqlvar[0].sqllen = sizeof(ISC_LONG);
	sqlda->sqlvar[0].sqlind = NULL;

	if (isc_dsql_execute(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 1, sqlda)) {
		_php_ibase_error();
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
	ibase_db_link *link;
	ibase_trans *trans;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &link_arg, &attachment_id) == FAILURE) {
		return;
	}

	PHP_IBASE_LINK_TRANS(link_arg, link, trans);

	if (_fbird_exec_kill(link, trans, (ISC_LONG) attachment_id) == FAILURE) {
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
	ibase_db_link *link;
	ibase_trans *trans;
	void *stmt = 0;
	XSQLDA *in_sqlda = NULL, *out_sqlda = NULL;
	char *pattern = NULL;

	RESET_ERRMSG;

	/* SQL to find attachments using the table in statements */
	static const char *sql =
		"SELECT DISTINCT A.MON$ATTACHMENT_ID, A.MON$USER "
		"FROM MON$ATTACHMENTS A "
		"JOIN MON$STATEMENTS S ON S.MON$ATTACHMENT_ID = A.MON$ATTACHMENT_ID "
		"WHERE A.MON$ATTACHMENT_ID <> CURRENT_CONNECTION "
		"AND UPPER(S.MON$SQL_TEXT) LIKE UPPER(?)";

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &link_arg, &table_name, &table_name_len) == FAILURE) {
		return;
	}

	PHP_IBASE_LINK_TRANS(link_arg, link, trans);

	if (isc_dsql_allocate_statement(IB_STATUS, &link->handle.db, (isc_stmt_handle*)&stmt)) {
		_php_ibase_error();
		RETURN_FALSE;
	}

	in_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
	in_sqlda->version = SQLDA_CURRENT_VERSION;
	in_sqlda->sqln = 1;

	if (isc_dsql_prepare(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 0, (char *)sql, 1, in_sqlda)) {
		_php_ibase_error();
		goto cleanup_error;
	}

    /* Prepare search pattern: %NAME% */
    pattern = emalloc(table_name_len + 3);
    snprintf(pattern, table_name_len + 3, "%%%s%%", table_name);

    /* Bind input */
	in_sqlda->sqlvar[0].sqldata = pattern;
	in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
	in_sqlda->sqlvar[0].sqllen = (short)(table_name_len + 2);
	in_sqlda->sqlvar[0].sqlind = NULL;

    /* Prepare output */
    out_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(2));
    out_sqlda->version = SQLDA_CURRENT_VERSION;
    out_sqlda->sqln = 2;

    if (isc_dsql_describe(IB_STATUS, (isc_stmt_handle*)&stmt, 1, out_sqlda)) {
        _php_ibase_error();
        goto cleanup_error;
    }

    /* Allocate buffers for output */
    ISC_LONG ret_id;
    char ret_user[256];
    short null_ind[2];

    out_sqlda->sqlvar[0].sqldata = (char *)&ret_id;
    out_sqlda->sqlvar[0].sqltype = SQL_LONG;
    out_sqlda->sqlvar[0].sqllen = sizeof(ISC_LONG);
    out_sqlda->sqlvar[0].sqlind = &null_ind[0];

    out_sqlda->sqlvar[1].sqldata = ret_user;
    out_sqlda->sqlvar[1].sqltype = SQL_TEXT;
    out_sqlda->sqlvar[1].sqllen = 255;
    out_sqlda->sqlvar[1].sqlind = &null_ind[1];

    if (isc_dsql_execute(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 1, in_sqlda)) {
        _php_ibase_error();
        goto cleanup_error;
    }

    array_init(return_value);

    while (1) {
        if (isc_dsql_fetch(IB_STATUS, (isc_stmt_handle*)&stmt, 1, out_sqlda)) {
            if (IB_STATUS[1] == 100) break; // EOF
            _php_ibase_error();
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
    if (pattern) efree(pattern);
    return;

cleanup_error:
    isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);
    if (in_sqlda) efree(in_sqlda);
    if (out_sqlda) efree(out_sqlda);
    if (pattern) efree(pattern);
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
	ibase_db_link *link;
	ibase_trans *trans;
    void *stmt = 0;
    XSQLDA *in_sqlda = NULL, *out_sqlda = NULL;
    char *pattern = NULL;
    ISC_LONG *kill_list = NULL;
    char *drop_sql = NULL;

    RESET_ERRMSG;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &link_arg, &table_name, &table_name_len) == FAILURE) {
		return;
	}

    PHP_IBASE_LINK_TRANS(link_arg, link, trans);

    /* 1. Collect Blockers */
    pattern = emalloc(table_name_len + 3);
    snprintf(pattern, table_name_len + 3, "%%%s%%", table_name);

    static const char *sql =
		"SELECT DISTINCT A.MON$ATTACHMENT_ID "
		"FROM MON$ATTACHMENTS A "
		"JOIN MON$STATEMENTS S ON S.MON$ATTACHMENT_ID = A.MON$ATTACHMENT_ID "
		"WHERE A.MON$ATTACHMENT_ID <> CURRENT_CONNECTION "
		"AND UPPER(S.MON$SQL_TEXT) LIKE UPPER(?)";

    if (isc_dsql_allocate_statement(IB_STATUS, &link->handle.db, (isc_stmt_handle*)&stmt)) {
		_php_ibase_error();
        goto error;
	}

    in_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
	in_sqlda->version = SQLDA_CURRENT_VERSION;
	in_sqlda->sqln = 1;

	if (isc_dsql_prepare(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 0, (char *)sql, 1, in_sqlda)) {
		_php_ibase_error();
		goto error;
	}

    in_sqlda->sqlvar[0].sqldata = pattern;
    in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
    in_sqlda->sqlvar[0].sqllen = (short)(table_name_len + 2);
    in_sqlda->sqlvar[0].sqlind = NULL;

    out_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
    out_sqlda->version = SQLDA_CURRENT_VERSION;
    out_sqlda->sqln = 1;

    if (isc_dsql_describe(IB_STATUS, (isc_stmt_handle*)&stmt, 1, out_sqlda)) {
         _php_ibase_error();
         goto error;
    }

    ISC_LONG ret_id;
    short null_ind;

    out_sqlda->sqlvar[0].sqldata = (char *)&ret_id;
    out_sqlda->sqlvar[0].sqltype = SQL_LONG;
    out_sqlda->sqlvar[0].sqllen = sizeof(ISC_LONG);
    out_sqlda->sqlvar[0].sqlind = &null_ind;

    if (isc_dsql_execute(IB_STATUS, &trans->handle.tr, (isc_stmt_handle*)&stmt, 1, in_sqlda)) {
        _php_ibase_error();
        goto error;
    }

    int kill_list_size = 10;
    int kill_list_count = 0;
    kill_list = emalloc(sizeof(ISC_LONG) * kill_list_size);

    while (1) {
        if (isc_dsql_fetch(IB_STATUS, (isc_stmt_handle*)&stmt, 1, out_sqlda)) {
             if (IB_STATUS[1] == 100) break;
             _php_ibase_error();
             /* Break on error but attempt kill of what we found? Or abort? Abort safer. */
             goto error;
        }
        if (kill_list_count >= kill_list_size) {
            kill_list_size *= 2;
            kill_list = erealloc(kill_list, sizeof(ISC_LONG) * kill_list_size);
        }
        kill_list[kill_list_count++] = ret_id;
    }

    /* 2. Cleanup Query Resources */
    isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);
    stmt = 0;

    /* 3. Execute Kills */
    for(int i=0; i<kill_list_count; i++) {
        _fbird_exec_kill(link, trans, kill_list[i]);
    }

    /* 4. Execute Drop */
    int len = spprintf(&drop_sql, 0, "DROP TABLE %s", table_name);

    if (isc_dsql_allocate_statement(IB_STATUS, &link->handle.db, (isc_stmt_handle*)&stmt)) {
         _php_ibase_error();
         goto error;
    }

    /* Use execute immediate for DDL (no params) */
    if (isc_dsql_execute_immediate(IB_STATUS, &link->handle.db, &trans->handle.tr, len, drop_sql, 1, NULL)) {
         _php_ibase_error();
         goto error;
    }

    /* Success Path */
    isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);

    if (in_sqlda) efree(in_sqlda);
    if (out_sqlda) efree(out_sqlda);
    if (pattern) efree(pattern);
    if (kill_list) efree(kill_list);
    if (drop_sql) efree(drop_sql);

    RETURN_TRUE;

error:
    if (stmt) isc_dsql_free_statement(IB_STATUS, (isc_stmt_handle*)&stmt, DSQL_drop);
    if (in_sqlda) efree(in_sqlda);
    if (out_sqlda) efree(out_sqlda);
    if (pattern) efree(pattern);
    if (kill_list) efree(kill_list);
    if (drop_sql) efree(drop_sql);
    RETURN_FALSE;
}
/* }}} */

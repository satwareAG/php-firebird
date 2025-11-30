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
#include "php_interbase.h"
#include "php_ibase_includes.h"

/* Helper to execute a delete statement with one integer parameter */
static int _fbird_exec_kill(ibase_db_link *link, ibase_trans *trans, ISC_LONG attachment_id)
{
	isc_stmt_handle stmt = 0;
	ISC_STATUS status[20];
	XSQLDA *sqlda;
	static const char *sql = "DELETE FROM MON$ATTACHMENTS WHERE MON$ATTACHMENT_ID = ?";

	if (isc_dsql_allocate_statement(status, &link->handle, &stmt)) {
		_php_ibase_error();
		return FAILURE;
	}

	sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
	sqlda->version = SQLDA_CURRENT_VERSION;
	sqlda->sqln = 1;

	if (isc_dsql_prepare(status, &trans->handle, &stmt, 0, (char *)sql, 1, sqlda)) {
		_php_ibase_error();
		isc_dsql_free_statement(status, &stmt, DSQL_drop);
		efree(sqlda);
		return FAILURE;
	}

	/* Bind parameter */
	sqlda->sqlvar[0].sqldata = (char *)&attachment_id;
	sqlda->sqlvar[0].sqltype = SQL_LONG;
	sqlda->sqlvar[0].sqllen = sizeof(ISC_LONG);
	sqlda->sqlvar[0].sqlind = NULL;

	if (isc_dsql_execute(status, &trans->handle, &stmt, 1, sqlda)) {
		_php_ibase_error();
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
		efree(sqlda);
		return FAILURE;
	}

	isc_dsql_free_statement(status, &stmt, DSQL_drop);
	efree(sqlda);
	return SUCCESS;
}

/* {{{ proto bool fbird_kill_attachment(resource link_or_trans, int attachment_id)
   Terminates a specific connection */
PHP_FUNCTION(fbird_kill_attachment)
{
	zval *link_arg;
	zend_long attachment_id;
	ibase_db_link *link;
	ibase_trans *trans;

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
	isc_stmt_handle stmt = 0;
	ISC_STATUS status[20];
	XSQLDA *in_sqlda, *out_sqlda;

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

	if (isc_dsql_allocate_statement(status, &link->handle, &stmt)) {
		_php_ibase_error();
		RETURN_FALSE;
	}

	in_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
	in_sqlda->version = SQLDA_CURRENT_VERSION;
	in_sqlda->sqln = 1;

	if (isc_dsql_prepare(status, &trans->handle, &stmt, 0, (char *)sql, 1, in_sqlda)) {
		_php_ibase_error();
		isc_dsql_free_statement(status, &stmt, DSQL_drop);
		efree(in_sqlda);
		RETURN_FALSE;
	}

    /* Prepare search pattern: %NAME% */
    /* We accept table names. Usually in SQL they appear as " NAME " or just NAME.
       %NAME% is heuristic but acceptable for this utility function */
    char *pattern = emalloc(table_name_len + 3);
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

    if (isc_dsql_describe(status, &stmt, 1, out_sqlda)) {
        _php_ibase_error();
        goto cleanup;
    }

    /* Allocate buffers for output */
    ISC_LONG ret_id;
    char ret_user[256];
    short null_ind[2];

    out_sqlda->sqlvar[0].sqldata = (char *)&ret_id;
    out_sqlda->sqlvar[0].sqltype = SQL_LONG; // force to LONG
    out_sqlda->sqlvar[0].sqllen = sizeof(ISC_LONG);
    out_sqlda->sqlvar[0].sqlind = &null_ind[0];

    out_sqlda->sqlvar[1].sqldata = ret_user;
    out_sqlda->sqlvar[1].sqltype = SQL_TEXT;
    out_sqlda->sqlvar[1].sqllen = 255;
    out_sqlda->sqlvar[1].sqlind = &null_ind[1];

    if (isc_dsql_execute(status, &trans->handle, &stmt, 1, in_sqlda)) {
        _php_ibase_error();
        goto cleanup;
    }

    array_init(return_value);

    while (1) {
        if (isc_dsql_fetch(status, &stmt, 1, out_sqlda)) {
            if (status[1] == 100) break; // EOF
            _php_ibase_error();
            goto cleanup;
        }

        zval row;
        array_init(&row);
        add_assoc_long(&row, "attachment_id", ret_id);

        /* Trim user field */
        // Note: SQL_TEXT is space padded.
        int info_len = out_sqlda->sqlvar[1].sqllen;
        // Actually we hardcoded 255 size but sqllen might be different reported by describe,
        // but we didn't call describeBind? No we did describe. But we forced sqldata buffers.
        // Wait, describe outputs info about columns. We should allocate based on that or coercion.
        // Coercion via sqlda->sqlvar[i].sqltype = SQL_TEXT IS supported.
        ret_user[out_sqlda->sqlvar[1].sqllen] = '\0'; // rough safety

        // Trim trailing spaces manually
        for (int i = out_sqlda->sqlvar[1].sqllen - 1; i >= 0; i--) {
            if (ret_user[i] == ' ') ret_user[i] = '\0';
            else break;
        }

        add_assoc_string(&row, "user", ret_user);
        add_next_index_zval(return_value, &row);
    }

cleanup:
    isc_dsql_free_statement(status, &stmt, DSQL_drop);
    efree(in_sqlda);
    efree(out_sqlda);
    efree(pattern);
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

    /* Reusing logic involves calling parsing again or calling C function helpers?
       We can call our own C functions fbird_list_table_blockers provided we refactor logic out of PHP_FUNCTION macros.
       Or just implement directly here. */

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &link_arg, &table_name, &table_name_len) == FAILURE) {
		return;
	}

    /* Since this function is complex (List -> Loop -> Kill -> Drop),
       and we want to avoid duplicating huge chunks of DSQL code,
       It may be better to implement the logic:
       1. Get Blockers List
       2. Loop and Kill
       3. Drop
    */

    /*
    WARNING: We cannot easily call PHP_FUNCTION(fbird_kill_attachment) from here without overhead.
    But we have _fbird_exec_kill helper!
    And we can duplicate the listing logic or extract it.
    For the sake of "Act Mode" efficiency, I'll skip extraction for now and do a targeted fetch-loop-kill.
    */

    PHP_IBASE_LINK_TRANS(link_arg, link, trans);

    char *pattern = emalloc(table_name_len + 3);
    snprintf(pattern, table_name_len + 3, "%%%s%%", table_name);

    isc_stmt_handle stmt = 0;
    ISC_STATUS status[20];
    static const char *sql =
		"SELECT DISTINCT A.MON$ATTACHMENT_ID "
		"FROM MON$ATTACHMENTS A "
		"JOIN MON$STATEMENTS S ON S.MON$ATTACHMENT_ID = A.MON$ATTACHMENT_ID "
		"WHERE A.MON$ATTACHMENT_ID <> CURRENT_CONNECTION "
		"AND UPPER(S.MON$SQL_TEXT) LIKE UPPER(?)";

    if (isc_dsql_allocate_statement(status, &link->handle, &stmt)) {
		_php_ibase_error();
        efree(pattern);
		RETURN_FALSE;
	}

    XSQLDA *in_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
	in_sqlda->version = SQLDA_CURRENT_VERSION;
	in_sqlda->sqln = 1;

	if (isc_dsql_prepare(status, &trans->handle, &stmt, 0, (char *)sql, 1, in_sqlda)) {
		_php_ibase_error();
		isc_dsql_free_statement(status, &stmt, DSQL_drop);
		efree(in_sqlda);
        efree(pattern);
		RETURN_FALSE;
	}

    in_sqlda->sqlvar[0].sqldata = pattern;
    in_sqlda->sqlvar[0].sqltype = SQL_TEXT;
    in_sqlda->sqlvar[0].sqllen = (short)(table_name_len + 2);
    in_sqlda->sqlvar[0].sqlind = NULL;

    XSQLDA *out_sqlda = (XSQLDA *) emalloc(XSQLDA_LENGTH(1));
    out_sqlda->version = SQLDA_CURRENT_VERSION;
    out_sqlda->sqln = 1;

    if (isc_dsql_describe(status, &stmt, 1, out_sqlda)) {
         _php_ibase_error();
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        efree(in_sqlda);
        efree(pattern);
        efree(out_sqlda);
        RETURN_FALSE;
    }

    ISC_LONG ret_id;
    short null_ind;

    out_sqlda->sqlvar[0].sqldata = (char *)&ret_id;
    out_sqlda->sqlvar[0].sqltype = SQL_LONG;
    out_sqlda->sqlvar[0].sqllen = sizeof(ISC_LONG);
    out_sqlda->sqlvar[0].sqlind = &null_ind;

    if (isc_dsql_execute(status, &trans->handle, &stmt, 1, in_sqlda)) {
        _php_ibase_error();
         isc_dsql_free_statement(status, &stmt, DSQL_drop);
        efree(in_sqlda);
        efree(pattern);
        efree(out_sqlda);
        RETURN_FALSE;
    }

    /* Store IDs to kill to avoid messing with cursor while deleting rows from same table?
       MON$ tables are virtual, but better safe. */
    int kill_list_size = 10;
    int kill_list_count = 0;
    ISC_LONG *kill_list = emalloc(sizeof(ISC_LONG) * kill_list_size);

    while (1) {
        if (isc_dsql_fetch(status, &stmt, 1, out_sqlda)) {
             if (status[1] == 100) break;
             _php_ibase_error();
             // continue or break? break
             break;
        }
        if (kill_list_count >= kill_list_size) {
            kill_list_size *= 2;
            kill_list = erealloc(kill_list, sizeof(ISC_LONG) * kill_list_size);
        }
        kill_list[kill_list_count++] = ret_id;
    }

    isc_dsql_free_statement(status, &stmt, DSQL_drop);
    efree(in_sqlda);
    efree(out_sqlda);
    efree(pattern);

    /* Kill them */
    for(int i=0; i<kill_list_count; i++) {
        _fbird_exec_kill(link, trans, kill_list[i]);
        /* Ignore errors on kill (maybe already gone) */
    }
    efree(kill_list);

    /* Now Drop Table */
    char *drop_sql;
    /* We can use _php_ibase_exec helper from ibase_query.c? No it's static or tied to INTERNAL params.
       We create a simple execute DDL helper logic here. */

    int len = spprintf(&drop_sql, 0, "DROP TABLE %s", table_name);

    stmt = 0;
    if (isc_dsql_allocate_statement(status, &link->handle, &stmt)) {
         _php_ibase_error();
         efree(drop_sql);
         RETURN_FALSE;
    }

    if (isc_dsql_execute_immediate(status, &link->handle, &trans->handle, len, drop_sql, 1, NULL)) {
         _php_ibase_error();
         efree(drop_sql);
         isc_dsql_free_statement(status, &stmt, DSQL_drop);
         RETURN_FALSE;
    }

    efree(drop_sql);
    isc_dsql_free_statement(status, &stmt, DSQL_drop);

    RETURN_TRUE;
}
/* }}} */

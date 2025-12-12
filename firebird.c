/*
   +----------------------------------------------------------------------+
   | PHP Version 7, 8                                                     |
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
   | Authors: Jouni Ahto <jouni.ahto@exdec.fi>                            |
   |          Andrew Avdeev <andy@simgts.mv.ru>                           |
   |          Ard Biesheuvel <a.k.biesheuvel@its.tudelft.nl>              |
   |          Martin Koeditz <martin.koeditz@it-syn.de>                   |
   |          Martins Lazdans <marrtins@dqdp.net>                         |
   |          Jane Alesi <ja@satware.ai>                                  |
   |          others                                                      |
   +----------------------------------------------------------------------+
   | You'll find history on Github                                        |
   | https://github.com/FirebirdSQL/php-firebird/commits/master           |
   +----------------------------------------------------------------------+
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#if HAVE_FIREBIRD

#include "php_ini.h"
#include "ext/standard/php_standard.h"
#include "ext/standard/md5.h"
#include "php_firebird.h"
#include "php_fbird_includes.h"
#include "php_fbird_inspection.h"
#include "SAPI.h"
#include "zend_exceptions.h"
#include <stdbool.h>
#include <time.h>
#include "firebird_utils.h"

#define ROLLBACK    0
#define COMMIT      1
#define RETAIN      2

#define CHECK_LINK(link) { if (link==NULL) { php_error_docref(NULL, E_WARNING, "A link to the server could not be established"); RETURN_FALSE; } }

ZEND_DECLARE_MODULE_GLOBALS(fbird)
static PHP_GINIT_FUNCTION(fbird);

zend_class_entry *firebird_exception_ce;

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO(arginfo_fbird_errmsg, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fbird_errcode, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_connect, 0, 0, 0)
	ZEND_ARG_INFO(0, database)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, charset)
	ZEND_ARG_INFO(0, buffers)
	ZEND_ARG_INFO(0, dialect)
	ZEND_ARG_INFO(0, role)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_pconnect, 0, 0, 0)
	ZEND_ARG_INFO(0, database)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, charset)
	ZEND_ARG_INFO(0, buffers)
	ZEND_ARG_INFO(0, dialect)
	ZEND_ARG_INFO(0, role)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_close, 0, 0, 0)
	ZEND_ARG_TYPE_INFO(0, link_identifier, IS_RESOURCE, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_drop_db, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_trans, 0, 0, 0)
	ZEND_ARG_VARIADIC_INFO(0, trans_args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_trans_start, 0, 0, 1)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, options, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_savepoint, 0, 0, 2)
	ZEND_ARG_INFO(0, trans_handle)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_trans_info, 0, 0, 1)
	ZEND_ARG_INFO(0, trans_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_commit, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_rollback, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_commit_ret, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_rollback_ret, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_gen_id, 0, 0, 1)
	ZEND_ARG_INFO(0, generator)
	ZEND_ARG_INFO(0, increment)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_create, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_open, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, blob_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_add, 0, 0, 2)
	ZEND_ARG_INFO(0, blob_handle)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_get, 0, 0, 2)
	ZEND_ARG_INFO(0, blob_handle)
	ZEND_ARG_INFO(0, len)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_close, 0, 0, 1)
	ZEND_ARG_INFO(0, blob_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_cancel, 0, 0, 1)
	ZEND_ARG_INFO(0, blob_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_info, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, blob_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_echo, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, blob_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_import, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, file)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_query, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, query)
	ZEND_ARG_VARIADIC_INFO(0, bind_arg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_affected_rows, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_fetch_row, 0, 0, 1)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_INFO(0, fetch_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_fetch_assoc, 0, 0, 1)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_INFO(0, fetch_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_fetch_object, 0, 0, 1)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_INFO(0, fetch_flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_name_result, 0, 0, 2)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_free_result, 0, 0, 1)
	ZEND_ARG_INFO(0, result)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_prepare, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, query)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_execute, 0, 0, 1)
	ZEND_ARG_INFO(0, query)
	ZEND_ARG_VARIADIC_INFO(0, bind_arg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_free_query, 0, 0, 1)
	ZEND_ARG_INFO(0, query)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_list_table_blockers, 0, 0, 2)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_INFO(0, table_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_kill_attachment, 0, 0, 2)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_INFO(0, attachment_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_drop_table_force, 0, 0, 2)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_INFO(0, table_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_execute_statement, 0, 0, 2)
    ZEND_ARG_INFO(0, trans_handle)
    ZEND_ARG_INFO(0, query)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_execute_query, 0, 0, 2)
    ZEND_ARG_INFO(0, trans_handle)
    ZEND_ARG_INFO(0, query)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_execute_auto, 0, 0, 2)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_INFO(0, query)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_num_fields, 0, 0, 1)
	ZEND_ARG_INFO(0, query_result)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_field_info, 0, 0, 2)
	ZEND_ARG_INFO(0, query_result)
	ZEND_ARG_INFO(0, field_number)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_num_params, 0, 0, 1)
	ZEND_ARG_INFO(0, query)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_param_info, 0, 0, 2)
	ZEND_ARG_INFO(0, query)
	ZEND_ARG_INFO(0, field_number)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_add_user, 0, 0, 3)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_INFO(0, user_name)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, first_name)
	ZEND_ARG_INFO(0, middle_name)
	ZEND_ARG_INFO(0, last_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_modify_user, 0, 0, 3)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_INFO(0, user_name)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, first_name)
	ZEND_ARG_INFO(0, middle_name)
	ZEND_ARG_INFO(0, last_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_delete_user, 0, 0, 3)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_INFO(0, user_name)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, first_name)
	ZEND_ARG_INFO(0, middle_name)
	ZEND_ARG_INFO(0, last_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_service_attach, 0, 0, 0)
	ZEND_ARG_INFO(0, host)
	ZEND_ARG_INFO(0, dba_username)
	ZEND_ARG_INFO(0, dba_password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_service_detach, 0, 0, 1)
	ZEND_ARG_INFO(0, service_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_backup, 0, 0, 3)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_INFO(0, source_db)
	ZEND_ARG_INFO(0, dest_file)
	ZEND_ARG_INFO(0, options)
	ZEND_ARG_INFO(0, verbose)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_restore, 0, 0, 3)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_INFO(0, source_file)
	ZEND_ARG_INFO(0, dest_db)
	ZEND_ARG_INFO(0, options)
	ZEND_ARG_INFO(0, verbose)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_maintain_db, 0, 0, 3)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_INFO(0, db)
	ZEND_ARG_INFO(0, action)
	ZEND_ARG_INFO(0, argument)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_db_info, 0, 0, 3)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_INFO(0, db)
	ZEND_ARG_INFO(0, action)
	ZEND_ARG_INFO(0, argument)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_server_info, 0, 0, 2)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_INFO(0, action)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_wait_event, 0, 0, 1)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, event)
	ZEND_ARG_INFO(0, event2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_set_event_handler, 0, 0, 2)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, handler)
	ZEND_ARG_INFO(0, event)
	ZEND_ARG_INFO(0, event2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_poll_event, 0, 0, 1)
	ZEND_ARG_INFO(0, event)
	ZEND_ARG_INFO(0, timeout_ms)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_free_event_handler, 0, 0, 1)
	ZEND_ARG_INFO(0, event)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fbird_get_client_version, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fbird_get_client_major_version, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fbird_get_client_minor_version, 0)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ extension definition structures */
static const zend_function_entry fbird_functions[] = {
	PHP_FE(fbird_connect, 		arginfo_fbird_connect)
	PHP_FE(fbird_pconnect, 		arginfo_fbird_pconnect)
	PHP_FE(fbird_close, 		arginfo_fbird_close)
	PHP_FE(fbird_drop_db, 		arginfo_fbird_drop_db)
	PHP_FE(fbird_query, 		arginfo_fbird_query)
	PHP_FE(fbird_fetch_row, 	arginfo_fbird_fetch_row)
	PHP_FE(fbird_fetch_assoc, 	arginfo_fbird_fetch_assoc)
	PHP_FE(fbird_fetch_object, 	arginfo_fbird_fetch_object)
	PHP_FE(fbird_free_result, 	arginfo_fbird_free_result)
	PHP_FE(fbird_name_result, 	arginfo_fbird_name_result)
	PHP_FE(fbird_prepare, 		arginfo_fbird_prepare)
	PHP_FE(fbird_execute, 		arginfo_fbird_execute)
	PHP_FE(fbird_free_query, 	arginfo_fbird_free_query)

    PHP_FE(fbird_list_table_blockers, arginfo_fbird_list_table_blockers)
    PHP_FE(fbird_kill_attachment, arginfo_fbird_kill_attachment)
    PHP_FE(fbird_drop_table_force, arginfo_fbird_drop_table_force)

    PHP_FE(fbird_execute_statement, arginfo_fbird_execute_statement)
    PHP_FE(fbird_execute_query, arginfo_fbird_execute_query)
    PHP_FE(fbird_execute_auto, arginfo_fbird_execute_auto)

	PHP_FE(fbird_gen_id, 		arginfo_fbird_gen_id)
	PHP_FE(fbird_num_fields, 	arginfo_fbird_num_fields)
	PHP_FE(fbird_num_params, 	arginfo_fbird_num_params)
	PHP_FE(fbird_affected_rows, arginfo_fbird_affected_rows)
	PHP_FE(fbird_field_info, 	arginfo_fbird_field_info)
	PHP_FE(fbird_param_info, 	arginfo_fbird_param_info)

	PHP_FE(fbird_trans, 		arginfo_fbird_trans)
	PHP_FE(fbird_commit, 		arginfo_fbird_commit)
	PHP_FE(fbird_rollback, 		arginfo_fbird_rollback)
	PHP_FE(fbird_commit_ret, 	arginfo_fbird_commit_ret)
	PHP_FE(fbird_rollback_ret, 	arginfo_fbird_rollback_ret)

	PHP_FE(fbird_blob_info, 	arginfo_fbird_blob_info)
	PHP_FE(fbird_blob_create, 	arginfo_fbird_blob_create)
	PHP_FE(fbird_blob_add, 		arginfo_fbird_blob_add)
	PHP_FE(fbird_blob_cancel, 	arginfo_fbird_blob_cancel)
	PHP_FE(fbird_blob_close, 	arginfo_fbird_blob_close)
	PHP_FE(fbird_blob_open, 	arginfo_fbird_blob_open)
	PHP_FE(fbird_blob_get, 		arginfo_fbird_blob_get)
	PHP_FE(fbird_blob_echo, 	arginfo_fbird_blob_echo)
	PHP_FE(fbird_blob_import, 	arginfo_fbird_blob_import)
	PHP_FE(fbird_blob_create_stream,	arginfo_fbird_blob_create)
	PHP_FE(fbird_blob_open_stream, 	arginfo_fbird_blob_open)
	PHP_FE(fbird_errmsg, 		arginfo_fbird_errmsg)
	PHP_FE(fbird_errcode, 		arginfo_fbird_errcode)

	PHP_FE(fbird_add_user, 		arginfo_fbird_add_user)
	PHP_FE(fbird_modify_user, 	arginfo_fbird_modify_user)
	PHP_FE(fbird_delete_user, 	arginfo_fbird_delete_user)

	PHP_FE(fbird_service_attach, arginfo_fbird_service_attach)
	PHP_FE(fbird_service_detach, arginfo_fbird_service_detach)
	PHP_FE(fbird_backup, 		arginfo_fbird_backup)
	PHP_FE(fbird_restore, 		arginfo_fbird_restore)
	PHP_FE(fbird_maintain_db, 	arginfo_fbird_maintain_db)
	PHP_FE(fbird_db_info, 		arginfo_fbird_db_info)
	PHP_FE(fbird_server_info, 	arginfo_fbird_server_info)

	PHP_FE(fbird_wait_event, 			arginfo_fbird_wait_event)
	PHP_FE(fbird_set_event_handler, 	arginfo_fbird_set_event_handler)
	PHP_FE(fbird_poll_event, 			arginfo_fbird_poll_event)
	PHP_FE(fbird_free_event_handler, 	arginfo_fbird_free_event_handler)

	PHP_FE(fbird_get_client_version, arginfo_fbird_get_client_version)
	PHP_FE(fbird_get_client_major_version, arginfo_fbird_get_client_major_version)
	PHP_FE(fbird_get_client_minor_version, arginfo_fbird_get_client_minor_version)

	/**
	* These aliases are provided in order to maintain forward compatibility. As Firebird
	* and InterBase are developed independently, functionality might be different between
	* the two branches in future versions.
	* Firebird users should use the aliases, so future InterBase-specific changes will
	* not affect their code
	*/

	PHP_FE(fbird_trans_start,		arginfo_fbird_trans_start)
	PHP_FE(fbird_savepoint,			arginfo_fbird_savepoint)
	PHP_FE(fbird_rollback_savepoint,	arginfo_fbird_savepoint)
	PHP_FE(fbird_release_savepoint,	arginfo_fbird_savepoint)
	PHP_FE(fbird_trans_info,		arginfo_fbird_trans_info)






	PHP_FE_END
};

zend_module_entry firebird_module_entry = {
	STANDARD_MODULE_HEADER,
	"firebird",
	fbird_functions,
	PHP_MINIT(fbird),
	PHP_MSHUTDOWN(fbird),
	NULL,
	PHP_RSHUTDOWN(fbird),
	PHP_MINFO(fbird),
	PHP_FIREBIRD_VER_STR,
	PHP_MODULE_GLOBALS(fbird),
	PHP_GINIT(fbird),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_FIREBIRD
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(firebird)
#endif

/* True globals, no need for thread safety */
int le_link, le_plink, le_trans;

/* }}} */

/* error handling ---------------------------- */

/* {{{ proto fbird_errmsg(void)
   Return error message */
PHP_FUNCTION(fbird_errmsg)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (IBG(sql_code) != 0) {
		RETURN_STRING(IBG(errmsg));
	}

	RETURN_FALSE;
}
/* }}} */

/* {{{ proto fbird_get_client_version(void)
   Return client version in form major.minor */
PHP_FUNCTION(fbird_get_client_version)
{
	RETURN_DOUBLE((double)IBG(client_major_version) + (double)IBG(client_minor_version) / 10);
}
/* }}} */

/* {{{ proto fbird_get_client_major_version(void)
   Return client major version */
PHP_FUNCTION(fbird_get_client_major_version)
{
	RETURN_LONG(IBG(client_major_version));
}
/* }}} */

/* {{{ proto fbird_get_client_minor_version(void)
   Return client minor version */
PHP_FUNCTION(fbird_get_client_minor_version)
{
	RETURN_LONG(IBG(client_minor_version));
}
/* }}} */

/* {{{ proto fbird_errcode(void)
   Return error code */
PHP_FUNCTION(fbird_errcode)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	if (IBG(sql_code) != 0) {
		RETURN_LONG(IBG(sql_code));
	}
	RETURN_FALSE;
}
/* }}} */

/* print firebird error and save it for fbird_errmsg() */
void _php_fbird_error(void) /* {{{ */
{
	char *s = IBG(errmsg);
	const ISC_STATUS *statusp = IB_STATUS;
	size_t msg_len;

	IBG(sql_code) = isc_sqlcode(IB_STATUS);

	msg_len = strlen(IBG(errmsg));
	while (msg_len < MAX_ERRMSG && fb_interpret(s, MAX_ERRMSG - msg_len - 1, &statusp)) {
		msg_len = strlen(s);
		s[msg_len] = ' ';
		s[msg_len + 1] = '\0';
		msg_len = s - IBG(errmsg) + msg_len + 1;
		s = IBG(errmsg) + msg_len;
	}

	if (INI_BOOL("fbird.enable_exceptions")) {
		zend_throw_exception(firebird_exception_ce, IBG(errmsg), IBG(sql_code));
	} else {
		php_error_docref(NULL, E_WARNING, "%s", IBG(errmsg));
	}
}
/* }}} */

/* print php firebird module error and save it for fbird_errmsg() */
void _php_fbird_module_error(const char *msg, ...) /* {{{ */
{
	va_list ap;

	va_start(ap, msg);

	/* vsnprintf NUL terminates the buf and writes at most n-1 chars+NUL */
	vsnprintf(IBG(errmsg), MAX_ERRMSG, msg, ap);
	va_end(ap);

	IBG(sql_code) = -999; /* no SQL error */

	if (INI_BOOL("fbird.enable_exceptions")) {
		zend_throw_exception(firebird_exception_ce, IBG(errmsg), IBG(sql_code));
	} else {
		php_error_docref(NULL, E_WARNING, "%s", IBG(errmsg));
	}
}
/* }}} */

/* {{{ internal macros, functions and structures */
typedef struct {
	isc_db_handle *db_ptr;
	zend_long tpb_len;
	char *tpb_ptr;
} ISC_TEB;

/* }}} */

/* Fill ib_link and trans with the correct database link and transaction. */
void _php_fbird_get_link_trans(INTERNAL_FUNCTION_PARAMETERS, /* {{{ */
	zval *link_id, fbird_db_link **ib_link, fbird_transaction **trans)
{
	IBDEBUG("Transaction or database link?");
	if (Z_RES_P(link_id)->type == le_trans) {
		/* Transaction resource: make sure it refers to one link only, then
		   fetch it; database link is stored in ib_trans->db_link[]. */
		IBDEBUG("Type is le_trans");
		*trans = (fbird_transaction *)zend_fetch_resource_ex(link_id, LE_TRANS, le_trans);
		if ((*trans)->link_cnt > 1) {
			_php_fbird_module_error("Link id is ambiguous: transaction spans multiple connections."
				);
			return;
		}
		*ib_link = (*trans)->db_link[0];
		return;
	}
	IBDEBUG("Type is le_[p]link or id not found");
	/* Database link resource, use default transaction. */
	*trans = NULL;
	*ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_id, LE_LINK, le_link, le_plink);
}
/* }}} */

/* destructors ---------------------- */

static void _php_fbird_commit_link(fbird_db_link *link) /* {{{ */
{
	unsigned short i = 0, j;
	fbird_tr_list *l;
	fbird_event *e;
	IBDEBUG("Checking transactions to close...");

	for (l = link->tr_list; l != NULL; ++i) {
		fbird_tr_list *p = l;
		if (p->trans != 0) {
			if (i == 0) {
				if (p->trans->handle.ptr != 0) {
					IBDEBUG("Committing default transaction...");
					if (isc_commit_transaction(IB_STATUS, &p->trans->handle.tr)) {
						_php_fbird_error();
					}
				}
				efree(p->trans); /* default transaction is not a registered resource: clean up */
			} else {
				if (p->trans->handle.ptr != 0) {
					/* non-default trans might have been rolled back by other call of this dtor */
					IBDEBUG("Rolling back other transactions...");
					if (isc_rollback_transaction(IB_STATUS, &p->trans->handle.tr)) {
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

	for (e = link->event_head; e; e = e->event_next) {
		_php_fbird_free_event(e);
		e->link = NULL;
	}
}

/* }}} */

static void php_fbird_commit_link_rsrc(zend_resource *rsrc) /* {{{ */
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	_php_fbird_commit_link(link);
}
/* }}} */

static void _php_fbird_close_link(zend_resource *rsrc) /* {{{ */
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	_php_fbird_commit_link(link);

	/* Use OO API disconnect if connection was created via OO API */
	if (link->fbc_connection != NULL) {
		IBDEBUG("Closing normal link via OO API...");
		fbc_disconnect(link->fbc_connection, IB_STATUS);
		link->fbc_connection = NULL;
		link->handle.ptr = 0;
	} else if (link->handle.ptr != 0) {
		IBDEBUG("Closing normal link...");
		isc_detach_database(IB_STATUS, &link->handle.db);
	}
	IBG(num_links)--;
	efree(link);
}
/* }}} */

static void _php_fbird_close_plink(zend_resource *rsrc) /* {{{ */
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	_php_fbird_commit_link(link);

	/* Phase 3: Use OO API disconnect if connection was created via OO API */
	if (link->fbc_connection != NULL) {
		IBDEBUG("Closing permanent link via OO API...");
		fbc_disconnect(link->fbc_connection, IB_STATUS);
		link->fbc_connection = NULL;
		link->handle.ptr = 0;
	} else if (link->handle.ptr != 0) {
		IBDEBUG("Closing permanent link...");
		isc_detach_database(IB_STATUS, &link->handle.db);
	}
	IBG(num_persistent)--;
	IBG(num_links)--;
	free(link);
}
/* }}} */

static void _php_fbird_free_trans(zend_resource *rsrc) /* {{{ */
{
	fbird_transaction *trans = (fbird_transaction *)rsrc->ptr;
	unsigned short i;

	IBDEBUG("Cleaning up transaction resource...");

	/* Phase 4: Use OO API rollback when transaction was created via OO API */
	if (trans->fbt_transaction != NULL) {
		IBDEBUG("Rolling back unhandled OO API transaction...");
		if (fbt_rollback(trans->fbt_transaction, IB_STATUS)) {
			_php_fbird_error();
		}
		trans->fbt_transaction = NULL;
		trans->handle.ptr = 0;
	} else if (trans->handle.ptr != 0) {
		IBDEBUG("Rolling back unhandled transaction...");
		if (isc_rollback_transaction(IB_STATUS, &trans->handle.tr)) {
			_php_fbird_error();
		}
	}

	/* now remove this transaction from all the connection-transaction lists */
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
	efree(trans);
}
/* }}} */

/*
 * Custom INI display callback for password fields.
 * Displays "********" instead of the actual password value in phpinfo().
 * Note: This pattern is common across extensions; consider proposing for Zend API.
 */
static PHP_INI_DISP(php_fbird_password_displayer_cb)
{

	if ((type == PHP_INI_DISPLAY_ORIG && ini_entry->orig_value)
			|| (type == PHP_INI_DISPLAY_ACTIVE && ini_entry->value)) {
		PUTS("********");
	} else if (!sapi_module.phpinfo_as_text) {
		PUTS("<i>no value</i>");
	} else {
		PUTS("no value");
	}
}

#define PUTS_TP(str) do {       \
	if(has_puts) {              \
		PUTS(" | ");            \
	}                           \
	PUTS(str);                  \
	has_puts = 1;               \
} while (0)

static PHP_INI_DISP(php_fbird_trans_displayer)
{
	int has_puts = 0;
	char *value;

	if (type == ZEND_INI_DISPLAY_ORIG && ini_entry->modified) {
		value = ZSTR_VAL(ini_entry->orig_value);
	} else if (ini_entry->value) {
		value = ZSTR_VAL(ini_entry->value);
	} else {
		value = NULL;
	}

	if (value) {
		zend_long trans_argl = atol(value);

		if (trans_argl != PHP_IBASE_DEFAULT) {
			/* access mode */
			if (PHP_IBASE_READ == (trans_argl & PHP_IBASE_READ)) {
				PUTS_TP("IBASE_READ");
			} else if (PHP_IBASE_WRITE == (trans_argl & PHP_IBASE_WRITE)) {
				PUTS_TP("IBASE_WRITE");
			}

			/* isolation level */
			if (PHP_IBASE_COMMITTED == (trans_argl & PHP_IBASE_COMMITTED)) {
				PUTS_TP("IBASE_COMMITTED");
				if (PHP_IBASE_REC_VERSION == (trans_argl & PHP_IBASE_REC_VERSION)) {
					PUTS_TP("IBASE_REC_VERSION");
				} else if (PHP_IBASE_REC_NO_VERSION == (trans_argl & PHP_IBASE_REC_NO_VERSION)) {
					PUTS_TP("IBASE_REC_NO_VERSION");
				}
			} else if (PHP_IBASE_CONSISTENCY == (trans_argl & PHP_IBASE_CONSISTENCY)) {
				PUTS_TP("IBASE_CONSISTENCY");
			} else if (PHP_IBASE_CONCURRENCY == (trans_argl & PHP_IBASE_CONCURRENCY)) {
				PUTS_TP("IBASE_CONCURRENCY");
			}

			/* lock resolution */
			if (PHP_IBASE_NOWAIT == (trans_argl & PHP_IBASE_NOWAIT)) {
				PUTS_TP("IBASE_NOWAIT");
			} else if (PHP_IBASE_WAIT == (trans_argl & PHP_IBASE_WAIT)) {
				PUTS_TP("IBASE_WAIT");
				if (PHP_IBASE_LOCK_TIMEOUT == (trans_argl & PHP_IBASE_LOCK_TIMEOUT)) {
					PUTS_TP("IBASE_LOCK_TIMEOUT");
				}
			}
		} else {
			PUTS_TP("IBASE_DEFAULT");
		}
	}
}

/* {{{ startup, shutdown and info functions */
PHP_INI_BEGIN()
	PHP_INI_ENTRY_EX("fbird.allow_persistent", "1", PHP_INI_SYSTEM, NULL, zend_ini_boolean_displayer_cb)
	PHP_INI_ENTRY_EX("fbird.max_persistent", "-1", PHP_INI_SYSTEM, NULL, display_link_numbers)
	PHP_INI_ENTRY_EX("fbird.max_links", "-1", PHP_INI_SYSTEM, NULL, display_link_numbers)
	PHP_INI_ENTRY("fbird.default_db", NULL, PHP_INI_SYSTEM, NULL)
	PHP_INI_ENTRY("fbird.default_user", NULL, PHP_INI_ALL, NULL)
	PHP_INI_ENTRY_EX("fbird.default_password", NULL, PHP_INI_ALL, NULL, php_fbird_password_displayer_cb)
	PHP_INI_ENTRY("fbird.default_charset", NULL, PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("fbird.timestampformat", IB_DEF_DATE_FMT " " IB_DEF_TIME_FMT, PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("fbird.dateformat", IB_DEF_DATE_FMT, PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("fbird.timeformat", IB_DEF_TIME_FMT, PHP_INI_ALL, NULL)
	STD_PHP_INI_ENTRY_EX("fbird.default_trans_params", "0", PHP_INI_ALL, OnUpdateLongGEZero, default_trans_params, zend_fbird_globals, fbird_globals, php_fbird_trans_displayer)
	STD_PHP_INI_ENTRY_EX("fbird.default_lock_timeout", "0", PHP_INI_ALL, OnUpdateLongGEZero, default_lock_timeout, zend_fbird_globals, fbird_globals, display_link_numbers)
	STD_PHP_INI_ENTRY_EX("fbird.blob_segment_size", "4096", PHP_INI_ALL, OnUpdateLongGEZero, blob_segment_size, zend_fbird_globals, fbird_globals, display_link_numbers)
	PHP_INI_ENTRY_EX("fbird.enable_exceptions", "0", PHP_INI_ALL, NULL, zend_ini_boolean_displayer_cb)
PHP_INI_END()

#ifdef __GNUC__
void* _php_fbird_get_fbclient_symbol(const char* sym)
{
	return dlsym(RTLD_DEFAULT, sym);
}
#elif defined(PHP_WIN32)
void* _php_fbird_get_fbclient_symbol(const char* sym)
{
	HMODULE l = GetModuleHandle("fbclient");

	if (!l && !(l = GetModuleHandle("gds32"))) {
		return NULL;
	}

	return GetProcAddress(l, sym);
}
#else
	/*
	 * Compile-time check: This extension supports dynamic symbol lookup on:
	 * - POSIX systems with RTLD_DEFAULT (Linux, macOS, BSD, etc.)
	 * - Windows (GetModuleHandle/GetProcAddress)
	 *
	 * If you're porting to a new platform, implement _php_fbird_get_fbclient_symbol()
	 * to retrieve symbols from the loaded fbclient library.
	 */
	static_assert(false, "Platform not supported: implement _php_fbird_get_fbclient_symbol() for your platform");
#endif

static PHP_GINIT_FUNCTION(fbird)
{
#if defined(COMPILE_DL_FIREBIRD) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	fbird_globals->num_persistent = fbird_globals->num_links = 0;
	fbird_globals->sql_code = *fbird_globals->errmsg = 0;
	fbird_globals->default_link = NULL;
	fbird_globals->get_master_interface = _php_fbird_get_fbclient_symbol("fb_get_master_interface");
	fbird_globals->get_statement_interface = _php_fbird_get_fbclient_symbol("fb_get_statement_interface");

	if (fbird_globals->get_master_interface) {
		fbird_globals->master_instance = ((fb_get_master_interface_t)(fbird_globals->get_master_interface))();
		fbird_globals->client_version = fbu_get_client_version(fbird_globals->master_instance);
		fbird_globals->client_major_version = (fbird_globals->client_version >> 8) & 0xFF;
		fbird_globals->client_minor_version = fbird_globals->client_version & 0xFF;
	} else {
		fbird_globals->master_instance = NULL;
		fbird_globals->client_version = -1;
		fbird_globals->client_major_version = -1;
		fbird_globals->client_minor_version = -1;
	}
}

PHP_MINIT_FUNCTION(fbird)
{
	REGISTER_INI_ENTRIES();

	zend_class_entry ce;
	INIT_CLASS_ENTRY(ce, "Firebird\\Exception", NULL);
	firebird_exception_ce = zend_register_internal_class_ex(&ce, zend_ce_exception);

	le_link = zend_register_list_destructors_ex(_php_fbird_close_link, NULL, LE_LINK, module_number);
	le_plink = zend_register_list_destructors_ex(php_fbird_commit_link_rsrc, _php_fbird_close_plink, LE_PLINK, module_number);
	le_trans = zend_register_list_destructors_ex(_php_fbird_free_trans, NULL, LE_TRANS, module_number);

	/* Primary FBIRD_* constants (new naming convention) */
	REGISTER_LONG_CONSTANT("FBIRD_DEFAULT", PHP_IBASE_DEFAULT, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_CREATE", PHP_IBASE_CREATE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_TEXT", PHP_IBASE_FETCH_BLOBS, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_FETCH_BLOBS", PHP_IBASE_FETCH_BLOBS, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_FETCH_ARRAYS", PHP_IBASE_FETCH_ARRAYS, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_UNIXTIME", PHP_IBASE_UNIXTIME, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_VER", PHP_FIREBIRD_VER, CONST_PERSISTENT);

	/* transactions */
	REGISTER_LONG_CONSTANT("FBIRD_WRITE", PHP_IBASE_WRITE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_READ", PHP_IBASE_READ, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_COMMITTED", PHP_IBASE_COMMITTED, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_CONSISTENCY", PHP_IBASE_CONSISTENCY, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_CONCURRENCY", PHP_IBASE_CONCURRENCY, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_REC_VERSION", PHP_IBASE_REC_VERSION, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_REC_NO_VERSION", PHP_IBASE_REC_NO_VERSION, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_NOWAIT", PHP_IBASE_NOWAIT, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_WAIT", PHP_IBASE_WAIT, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_TIMEOUT", PHP_IBASE_LOCK_TIMEOUT, CONST_PERSISTENT);

	/* Table reservation constants */
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_SHARED", PHP_IBASE_LOCK_SHARED, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_PROTECTED", PHP_IBASE_LOCK_PROTECTED, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_EXCLUSIVE", PHP_IBASE_LOCK_EXCLUSIVE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_READ", PHP_IBASE_LOCK_READ, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_WRITE", PHP_IBASE_LOCK_WRITE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_READ_CONSISTENCY", PHP_IBASE_READ_CONSISTENCY, CONST_PERSISTENT);

	/* Event constants */
	REGISTER_LONG_CONSTANT("FBIRD_EVENT_TIMEOUT", PHP_IBASE_EVENT_TIMEOUT, CONST_PERSISTENT);


	php_fbird_query_minit(INIT_FUNC_ARGS_PASSTHRU);
	php_fbird_blobs_minit(INIT_FUNC_ARGS_PASSTHRU);
	php_fbird_events_minit(INIT_FUNC_ARGS_PASSTHRU);
	php_fbird_service_minit(INIT_FUNC_ARGS_PASSTHRU);

#ifdef ZEND_SIGNALS
	// firebird replaces some signals at runtime, suppress warnings.
	SIGG(check) = 0;
#endif

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(fbird)
{
#ifndef PHP_WIN32
	/**
	 * When the Interbase client API library libgds.so is first loaded, it registers a call to
	 * gds__cleanup() with atexit(), in order to clean up after itself when the process exits.
	 * This means that the library is called at process shutdown, and cannot be unloaded beforehand.
	 * PHP tries to unload modules after every request [dl()'ed modules], and right before the
	 * process shuts down [modules loaded from php.ini]. This results in a segfault for this module.
	 * By NULLing the dlopen() handle in the module entry, Zend omits the call to dlclose(),
	 * ensuring that the module will remain present until the process exits. However, the functions
	 * and classes exported by the module will not be available until the module is 'reloaded'.
	 * When reloaded, dlopen() will return the handle of the already loaded module. The module will
	 * be unloaded automatically when the process exits.
	 */
	zend_module_entry *fbird_entry;
	if ((fbird_entry = zend_hash_str_find_ptr(&module_registry, firebird_module_entry.name,
			strlen(firebird_module_entry.name))) != NULL) {
		fbird_entry->handle = 0;
	}
#endif
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(fbird)
{
	IBG(num_links) = IBG(num_persistent);
	IBG(default_link)= NULL;

	RESET_ERRMSG;

	return SUCCESS;
}

PHP_MINFO_FUNCTION(fbird)
{
	char tmp[64], *s;

	php_info_print_table_start();
	php_info_print_table_row(2, "Firebird Support",
#ifdef COMPILE_DL_FIREBIRD
		"dynamic");
#else
		"static");
#endif

	php_info_print_table_row(2, "Interbase extension version", PHP_FIREBIRD_VER_STR);

#ifdef FB_API_VER
	snprintf( (s = tmp), sizeof(tmp), "Firebird API version %d", FB_API_VER);
#elif (SQLDA_CURRENT_VERSION > 1)
	s =  "Interbase 7.0 and up";
#endif
	php_info_print_table_row(2, "Compile-time Client Library Version", s);

#if defined(__GNUC__) || defined(PHP_WIN32)
	do {
		info_func_t info_func = NULL;
#ifdef __GNUC__
		info_func = (info_func_t)dlsym(RTLD_DEFAULT, "isc_get_client_version");
#else
		HMODULE l = GetModuleHandle("fbclient");

		if (!l && !(l = GetModuleHandle("gds32"))) {
			break;
		}
		info_func = (info_func_t)GetProcAddress(l, "isc_get_client_version");
#endif
		if (info_func) {
			info_func(s = tmp);
		}
		php_info_print_table_row(2, "Run-time Client Library Version", s);
	} while (0);
#endif
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();

}
/* }}} */

enum connect_args { DB = 0, USER = 1, PASS = 2, CSET = 3, ROLE = 4, BUF = 0, DLECT = 1, SYNC = 2 };

static char const dpb_args[] = {
	0, isc_dpb_user_name, isc_dpb_password, isc_dpb_lc_ctype, isc_dpb_sql_role_name, 0
};

int _php_fbird_attach_db(char **args, size_t *len, zend_long *largs, void **db) /* {{{ */
{
    /*
     * Phase 3: OO API connection is available but disabled by default.
     *
     * The OO API IAttachment* is not directly compatible with legacy isc_db_handle
     * required by existing query/transaction functions (isc_dsql_*, isc_start_transaction, etc.).
     *
     * OO API connection will be enabled in Phase 4 when transaction and query layers
     * are also migrated to OO API. For now, OO API is used only for:
     * - Disconnect (handles both OO API and legacy connections)
     * - Drop database (via fbc_drop_database)
     *
     * The fbc_connect() function is available for future use and for operations
     * that need OO API connection (like createDatabase with extended options).
     */
    (void)0;  /* Placeholder - OO API connection disabled pending Phase 4 */

    /*
     * Legacy path: Build the DPB (database parameter buffer) using binary-safe writes.
     */
    unsigned char dpb_buffer[257];
    unsigned char *p = dpb_buffer;
    unsigned char *end = dpb_buffer + sizeof(dpb_buffer);
    short dpb_len;
    short i;

    /* DPB version */
    if (p >= end) {
        _php_fbird_module_error("DPB buffer too small");
        return FAILURE;
    }
    *p++ = isc_dpb_version1;

    /* Textual arguments: user, password, charset, role */
    for (i = 0; i < (short)sizeof(dpb_args); ++i) {
        if (dpb_args[i] && args[i] && len[i]) {
            size_t needed = 2 + len[i]; /* tag + length + payload */
            if ((size_t)(end - p) < needed) {
                /* Not enough space, stop appending further items. */
                break;
            }
            *p++ = (unsigned char)dpb_args[i];
            *p++ = (unsigned char)len[i];
            memcpy(p, args[i], len[i]);
            p += len[i];
        }
    }

    /* Numeric options: buffers */
    if (largs[BUF]) {
        if ((end - p) >= 4) {
            *p++ = isc_dpb_num_buffers;
            *p++ = 2; /* length */
            *p++ = (unsigned char)((largs[BUF] >> 8) & 0xff);
            *p++ = (unsigned char)(largs[BUF] & 0xff);
        }
    }

    /* Numeric options: force write sync/async */
    if (largs[SYNC]) {
        if ((end - p) >= 3) {
            *p++ = isc_dpb_force_write;
            *p++ = 1; /* length */
            *p++ = (unsigned char)(largs[SYNC] == isc_spb_prp_wm_sync);
        }
    }

#ifdef isc_dpb_set_bind
    /*
     * Bind compatibility settings for newer clients. Only send isc_dpb_set_bind
     * when fbclient exposes the master instance API (runtime capability check).
     * This avoids sending unknown DPB items to older servers (e.g. Firebird 3),
     * which can cause "Invalid clumplet buffer structure" during attach.
     */
    if (IBG(master_instance)) {
        const char *compat_buf;
        unsigned char compat_buf_size;

        /* If fbclient >= 4 then convert INT128/DECFLOAT to VARCHAR
         * Else (paranoia) include TIME ZONE to legacy mapping, though this
         * branch should not normally be taken if master_instance is absent. */
        if (IBG(client_major_version) >= 4) {
            static const char compat[] = "INT128 TO VARCHAR;DECFLOAT TO VARCHAR";
            compat_buf = compat;
            compat_buf_size = (unsigned char)(sizeof(compat) - 1);
        } else {
            static const char compat[] = "INT128 TO VARCHAR;DECFLOAT TO VARCHAR;TIME ZONE TO LEGACY";
            compat_buf = compat;
            compat_buf_size = (unsigned char)(sizeof(compat) - 1);
        }

        if ((end - p) >= (2 + (ptrdiff_t)compat_buf_size)) {
            *p++ = isc_dpb_set_bind;
            *p++ = compat_buf_size;
            memcpy(p, compat_buf, compat_buf_size);
            p += compat_buf_size;
        }
    }
#endif

    dpb_len = (short)(p - dpb_buffer);

    /* Clear the OO API connection slot when using legacy path */
    IBG(status[ISC_STATUS_LENGTH - 1]) = 0;

    if (isc_attach_database(IB_STATUS, (short)len[DB], args[DB], (isc_db_handle*)db, dpb_len, (char *)dpb_buffer)) {
        _php_fbird_error();
        return FAILURE;
    }
    return SUCCESS;
}
/* }}} */

static void _php_fbird_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent) /* {{{ */
{
	char *c, hash[16], *args[] = { NULL, NULL, NULL, NULL, NULL };
	int i;
	size_t len[] = { 0, 0, 0, 0, 0 };
	zend_long largs[] = { 0, 0, 0 };
	PHP_MD5_CTX hash_context;
	zend_resource new_index_ptr, *le;
	void *db_handle = 0;
	fbird_db_link *ib_link;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|ssssllsl",
			&args[DB], &len[DB], &args[USER], &len[USER], &args[PASS], &len[PASS],
			&args[CSET], &len[CSET], &largs[BUF], &largs[DLECT], &args[ROLE], &len[ROLE],
			&largs[SYNC])) {
		RETURN_FALSE;
	}

	/* restrict to the server/db in the .ini if in safe mode */
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

	/* try to reuse a connection */
	if ((le = zend_hash_str_find_ptr(&EG(regular_list), hash, sizeof(hash)-1)) != NULL) {
		zend_resource *xlink;

		if (le->type != le_index_ptr) {
			RETURN_FALSE;
		}

		xlink = (zend_resource*) le->ptr;
		if ((!persistent && xlink->type == le_link) || xlink->type == le_plink) {
			if (IBG(default_link) != xlink) {
				GC_ADDREF(xlink);
				if (IBG(default_link)) {
					zend_list_delete(IBG(default_link));
				}
				IBG(default_link) = xlink;
			}
			GC_ADDREF(xlink);
			RETURN_RES(xlink);
		} else {
			zend_hash_str_del(&EG(regular_list), hash, sizeof(hash)-1);
		}
	}

	/* ... or a persistent one */
	do {
		zend_long l;
		static char info[] = { isc_info_base_level, isc_info_end };
		char result[8];
		ISC_STATUS status[20];

		if ((le = zend_hash_str_find_ptr(&EG(persistent_list), hash, sizeof(hash)-1)) != NULL) {
			if (le->type != le_plink) {
				RETURN_FALSE;
			}
			/* check if connection has timed out */
			ib_link = (fbird_db_link *) le->ptr;
			if (!isc_database_info(status, &ib_link->handle.db, sizeof(info), info, sizeof(result), result)) {
				RETVAL_RES(zend_register_resource(ib_link, le_plink));
				break;
			}
			zend_hash_str_del(&EG(persistent_list), hash, sizeof(hash)-1);
		}

		/* no link found, so we have to open one */

		if ((l = INI_INT("fbird.max_links")) != -1 && IBG(num_links) >= l) {
			_php_fbird_module_error("Too many open links (%ld)", IBG(num_links));
			RETURN_FALSE;
		}

		/* create the ib_link */
		if (FAILURE == _php_fbird_attach_db(args, len, largs, &db_handle)) {
			RETURN_FALSE;
		}

		/* use non-persistent if allowed number of persistent links is exceeded */
		if (!persistent || ((l = INI_INT("fbird.max_persistent") != -1) && IBG(num_persistent) >= l)) {
			ib_link = (fbird_db_link *) emalloc(sizeof(fbird_db_link));
			RETVAL_RES(zend_register_resource(ib_link, le_link));
		} else {
			ib_link = (fbird_db_link *) malloc(sizeof(fbird_db_link));
			if (!ib_link) {
				RETURN_FALSE;
			}

			/* hash it up */
			if (zend_register_persistent_resource(hash, sizeof(hash)-1, ib_link, le_plink) == NULL) {
				free(ib_link);
				RETURN_FALSE;
			}
			RETVAL_RES(zend_register_resource(ib_link, le_plink));
			++IBG(num_persistent);
		}
		ib_link->handle.ptr = db_handle;
		ib_link->dialect = largs[DLECT] ? (unsigned short)largs[DLECT] : SQL_DIALECT_CURRENT;
		ib_link->tr_list = NULL;
		ib_link->event_head = NULL;

		/* Phase 3: Retrieve OO API connection pointer from _php_fbird_attach_db() */
		ib_link->fbc_connection = (void *)(uintptr_t)IBG(status[ISC_STATUS_LENGTH - 1]);
		IBG(status[ISC_STATUS_LENGTH - 1]) = 0;  /* Clear the temporary storage */

		++IBG(num_links);
	} while (0);

	/* add it to the hash */
	new_index_ptr.ptr = (void *) Z_RES_P(return_value);
	new_index_ptr.type = le_index_ptr;
	zend_hash_str_update_mem(&EG(regular_list), hash, sizeof(hash)-1,
			(void *) &new_index_ptr, sizeof(zend_resource));
	if (IBG(default_link)) {
		zend_list_delete(IBG(default_link));
	}
	IBG(default_link) = Z_RES_P(return_value);
	Z_TRY_ADDREF_P(return_value);
	Z_TRY_ADDREF_P(return_value);
}
/* }}} */

/* {{{ proto fbird_connect([string database [, string username [, string password [, string charset [, int buffers [, int dialect [, string role]]]]]]])
   Open a connection to an InterBase database */
PHP_FUNCTION(fbird_connect)
{
	_php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto fbird_pconnect([string database [, string username [, string password [, string charset [, int buffers [, int dialect [, string role]]]]]]])
   Open a persistent connection to an InterBase database */
PHP_FUNCTION(fbird_pconnect)
{
	_php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, INI_INT("fbird.allow_persistent"));
}
/* }}} */

/* Helper function for consolidated resource validation with proper error differentiation */
static int _php_fbird_validate_link_resource(zend_resource *link_res, bool is_default_link, bool clear_default) /* {{{ */
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
/* }}} */

/* Helper function for thread-safe default link adoption */
static void _php_fbird_adopt_new_default_link(zend_resource *closing_link) /* {{{ */
{
	/* Only search if we're actually clearing the current default */
	if (IBG(default_link) != closing_link) {
		return;
	}

	/* For now, simply clear the default. Full adoption logic can be added in future enhancement.
	 * This maintains existing behavior while providing the infrastructure for adoption. */
	IBG(default_link) = NULL;
}
/* }}} */

/* Helper function for optimized resource cleanup */
static void _php_fbird_close_resource(zend_resource *link_res) /* {{{ */
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
/* }}} */

/* {{{ proto bool fbird_close([resource link_identifier])
   Close an InterBase connection */
PHP_FUNCTION(fbird_close)
{
	zval *link_arg = NULL;
	zend_resource *link_res;
	bool is_default_link = false;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r!", &link_arg) == FAILURE) {
		return;
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
		/* Explicit link path */
		link_res = Z_RES_P(link_arg);
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
/* }}} */

/* {{{ proto fbird_drop_db([resource link_identifier])
   Drop an InterBase database */
PHP_FUNCTION(fbird_drop_db)
{
	zval *link_arg = NULL;
	fbird_db_link *ib_link;
	fbird_tr_list *l;
	zend_resource *link_res;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &link_arg) == FAILURE) {
		return;
	}

	if (ZEND_NUM_ARGS() == 0) {
		link_res = IBG(default_link);
		CHECK_LINK(link_res);
		IBG(default_link) = NULL;
	} else {
		link_res = Z_RES_P(link_arg);
	}

	ib_link = (fbird_db_link *)zend_fetch_resource2(link_res, LE_LINK, le_link, le_plink);

	if (!ib_link) {
		RETURN_FALSE;
	}

	if (isc_drop_database(IB_STATUS, &ib_link->handle.db)) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* isc_drop_database() doesn't invalidate the transaction handles */
	for (l = ib_link->tr_list; l != NULL; l = l->next) {
		if (l->trans != NULL) l->trans->handle.ptr = 0;
	}

	zend_list_delete(link_res);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto resource fbird_transaction([int trans_args [, resource link_identifier [, ... ], int trans_args [, resource link_identifier [, ... ]] [, ...]]])
   Start a transaction over one or several databases */

#define TPB_MAX_SIZE 2048

void _php_fbird_populate_trans(zend_long trans_argl, zend_long trans_timeout, char *last_tpb, unsigned short *len) /* {{{ */
{
	unsigned char *p = (unsigned char *) last_tpb;
	unsigned char *end = p + TPB_MAX_SIZE;

	/* No explicit flags: leave TPB empty so Firebird uses its defaults. */
	if (trans_argl == PHP_IBASE_DEFAULT) {
		*len = 0;
		return;
	}

	/* TPB version */
	if (p < end) *p++ = isc_tpb_version3;

	/* access mode */
	if (trans_argl & PHP_IBASE_READ) {
		if (p < end) *p++ = isc_tpb_read;
	} else if (trans_argl & PHP_IBASE_WRITE) {
		if (p < end) *p++ = isc_tpb_write;
	}

	/* isolation level */
	if (trans_argl & PHP_IBASE_COMMITTED) {
		if (p < end) *p++ = isc_tpb_read_committed;
		if (trans_argl & PHP_IBASE_REC_VERSION) {
			if (p < end) *p++ = isc_tpb_rec_version;
		} else if (trans_argl & PHP_IBASE_REC_NO_VERSION) {
			if (p < end) *p++ = isc_tpb_no_rec_version;
		}
	} else if (trans_argl & PHP_IBASE_CONSISTENCY) {
		if (p < end) *p++ = isc_tpb_consistency;
	} else if (trans_argl & PHP_IBASE_CONCURRENCY) {
		if (p < end) *p++ = isc_tpb_concurrency;
	}

	/* lock resolution */
	if (trans_argl & PHP_IBASE_NOWAIT) {
		if (p < end) *p++ = isc_tpb_nowait;
	} else if (trans_argl & PHP_IBASE_WAIT) {
		if (p < end) *p++ = isc_tpb_wait;
		if (trans_argl & PHP_IBASE_LOCK_TIMEOUT) {
			if (trans_timeout <= 0 || trans_timeout > 0x7FFF) {
				php_error_docref(NULL, E_WARNING, "Invalid timeout parameter (must be 0-32767)");
			} else {
				ISC_SHORT timeout = (ISC_SHORT) trans_timeout;
				if (p + 3 <= end) {
					*p++ = isc_tpb_lock_timeout;
					*p++ = (unsigned char) sizeof(ISC_SHORT);
					/* VAX/Firebird little-endian order */
					*p++ = (unsigned char) (timeout & 0xff);
					if (p < end) *p++ = (unsigned char) ((timeout >> 8) & 0xff);
				}
			}
		}
	}

	*len = (unsigned short) (p - (unsigned char *) last_tpb);
}
/* }}} */

void _php_fbird_populate_trans_from_array(zval *options, zend_long *trans_timeout, char *last_tpb, unsigned short *len) /* {{{ */
{
	unsigned char *p = (unsigned char *) last_tpb;
	unsigned char *end = p + TPB_MAX_SIZE;
	zval *tmp;

	/* TPB version */
	*p++ = isc_tpb_version3;

	/* access mode */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "access_mode", sizeof("access_mode") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			if (Z_LVAL_P(tmp) & PHP_IBASE_READ) {
				*p++ = isc_tpb_read;
			} else if (Z_LVAL_P(tmp) & PHP_IBASE_WRITE) {
				*p++ = isc_tpb_write;
			}
		}
	}

	/* isolation level */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "isolation", sizeof("isolation") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			zend_long iso = Z_LVAL_P(tmp);
			if (iso & PHP_IBASE_COMMITTED) {
				*p++ = isc_tpb_read_committed;
				if (iso & PHP_IBASE_REC_VERSION) {
					*p++ = isc_tpb_rec_version;
				} else if (iso & PHP_IBASE_REC_NO_VERSION) {
					*p++ = isc_tpb_no_rec_version;
				}
			} else if (iso & PHP_IBASE_CONSISTENCY) {
				*p++ = isc_tpb_consistency;
			} else if (iso & PHP_IBASE_CONCURRENCY) {
				*p++ = isc_tpb_concurrency;
			}
		}
	}

	/* lock resolution */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "lock_resolution", sizeof("lock_resolution") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			zend_long res = Z_LVAL_P(tmp);
			if (res & PHP_IBASE_NOWAIT) {
				*p++ = isc_tpb_nowait;
			} else if (res & PHP_IBASE_WAIT) {
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
					if (lock_mode & PHP_IBASE_LOCK_WRITE) {
						if (p < end) *p++ = isc_tpb_lock_write;
					} else if (lock_mode & PHP_IBASE_LOCK_READ) {
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
					if (lock_mode & PHP_IBASE_LOCK_PROTECTED) {
						if (p < end) *p++ = isc_tpb_protected;
					} else if (lock_mode & PHP_IBASE_LOCK_EXCLUSIVE) {
						if (p < end) *p++ = isc_tpb_exclusive;
					} else if (lock_mode & PHP_IBASE_LOCK_SHARED) {
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
/* }}} */

/* {{{ proto resource fbird_trans_start([resource link_identifier, ] array options)
   Start a transaction with array-based options */
PHP_FUNCTION(fbird_trans_start)
{
	zval *link_arg = NULL, *options_arg = NULL;
	fbird_db_link *ib_link;
	fbird_transaction *ib_trans;
	void *tr_handle = 0;
	ISC_STATUS result;
	char last_tpb[TPB_MAX_SIZE];
	unsigned short tpb_len = 0;
	zend_long trans_timeout = 0;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|ra", &link_arg, &options_arg) == FAILURE) {
		return;
	}

	/* If first arg is array, it's options, and use default link */
	if (link_arg && Z_TYPE_P(link_arg) == IS_ARRAY) {
		options_arg = link_arg;
		link_arg = NULL;
	}

	if (link_arg) {
		ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
	} else {
		ib_link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), LE_LINK, le_link, le_plink);
	}

	if (!ib_link) {
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

	result = isc_start_transaction(IB_STATUS, (isc_tr_handle*)&tr_handle, 1, &ib_link->handle.db, tpb_len, last_tpb);

	if (result) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	ib_trans = (fbird_transaction *) safe_emalloc(1-1, sizeof(fbird_db_link *), sizeof(fbird_transaction));
	ib_trans->handle.ptr = tr_handle;
	ib_trans->link_cnt = 1;
	ib_trans->affected_rows = 0;
	ib_trans->fbt_transaction = NULL;  /* Phase 4: Initialize OO API transaction pointer */
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

	RETVAL_RES(zend_register_resource(ib_trans, le_trans));
	Z_TRY_ADDREF_P(return_value);
}
/* }}} */

static void _php_fbird_exec_savepoint(INTERNAL_FUNCTION_PARAMETERS, const char *format) /* {{{ */
{
	zval *trans_arg = NULL;
	char *name;
	size_t name_len;
	fbird_transaction *trans;
	char *query;
	int len;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &trans_arg, &name, &name_len)) {
		return;
	}

	if (name_len == 0 || name_len > 31) { // Max identifier length
		php_error_docref(NULL, E_WARNING, "Invalid savepoint name (length must be 1-31 bytes)");
		RETURN_FALSE;
	}

	trans = (fbird_transaction *)zend_fetch_resource_ex(trans_arg, LE_TRANS, le_trans);
	if (!trans) {
		RETURN_FALSE;
	}

	/* Check if transaction involves exactly one connection */
	if (trans->link_cnt > 1) {
		_php_fbird_module_error("Savepoints not supported for multi-database transactions");
		RETURN_FALSE;
	}

	len = spprintf(&query, 0, format, name);

	if (isc_dsql_execute_immediate(IB_STATUS, &trans->db_link[0]->handle.db, &trans->handle.tr, 0, query,
			SQL_DIALECT_CURRENT, NULL)) {
		_php_fbird_error();
		efree(query);
		RETURN_FALSE;
	}

	efree(query);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool fbird_savepoint(resource trans_handle, string name)
   Create a named savepoint */
PHP_FUNCTION(fbird_savepoint)
{
	_php_fbird_exec_savepoint(INTERNAL_FUNCTION_PARAM_PASSTHRU, "SAVEPOINT %s");
}
/* }}} */

/* {{{ proto bool fbird_rollback_savepoint(resource trans_handle, string name)
   Rollback to a named savepoint */
PHP_FUNCTION(fbird_rollback_savepoint)
{
	_php_fbird_exec_savepoint(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ROLLBACK TO SAVEPOINT %s");
}
/* }}} */

/* {{{ proto bool fbird_release_savepoint(resource trans_handle, string name)
   Release a named savepoint */
PHP_FUNCTION(fbird_release_savepoint)
{
	_php_fbird_exec_savepoint(INTERNAL_FUNCTION_PARAM_PASSTHRU, "RELEASE SAVEPOINT %s");
}
/* }}} */

/* {{{ proto array fbird_trans_info(resource trans_handle)
   Return information about a transaction */
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

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "r", &trans_arg)) {
		RETURN_FALSE;
	}

	trans = (fbird_transaction *)zend_fetch_resource_ex(trans_arg, LE_TRANS, le_trans);
	if (!trans) {
		RETURN_FALSE;
	}

	if (isc_transaction_info(IB_STATUS, &trans->handle.tr, sizeof(tpb), tpb, sizeof(res_buf), res_buf)) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	array_init(return_value);

	while (*p != isc_info_end && p < res_buf + sizeof(res_buf)) {
		unsigned char item = *p++;
		unsigned short len = (unsigned short)isc_vax_integer(p, 2);
		p += 2;

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
/* }}} */

PHP_FUNCTION(fbird_trans)
{
	int i, argn = ZEND_NUM_ARGS();
	unsigned short link_cnt = 0, tpb_len = 0;
	char last_tpb[TPB_MAX_SIZE];
	fbird_db_link **ib_link = NULL;
	fbird_transaction *ib_trans;
	void *tr_handle = 0;
	ISC_STATUS result = 0;

	RESET_ERRMSG;

	/* (1+argn) is an upper bound for the number of links this trans connects to */
	ib_link = (fbird_db_link **) safe_emalloc(sizeof(fbird_db_link *),1+argn,0);

	if (argn > 0) {
		zend_long trans_argl = 0;
		zend_long trans_timeout = 0;
		char *tpb;
		ISC_TEB *teb;
		zval *args = NULL;

		if (zend_parse_parameters(argn, "+", &args, &argn) == FAILURE) {
			efree(ib_link);
			RETURN_FALSE;
		}

		teb = (ISC_TEB *) safe_emalloc(sizeof(ISC_TEB),argn,0);
		tpb = (char *) safe_emalloc(TPB_MAX_SIZE,argn,0);

		/* enumerate all the arguments: assume every non-resource argument
		   specifies modifiers for the link ids that follow it */
		for (i = 0; i < argn; ++i) {

			if (Z_TYPE(args[i]) == IS_RESOURCE) {

				if ((ib_link[link_cnt] = (fbird_db_link *)zend_fetch_resource2_ex(&args[i], LE_LINK, le_link, le_plink)) == NULL) {
					efree(teb);
					efree(tpb);
					efree(ib_link);
					RETURN_FALSE;
				}

				/* copy the most recent modifier string into tbp[] */
				memcpy(&tpb[TPB_MAX_SIZE * link_cnt], last_tpb, TPB_MAX_SIZE);

				/* add a database handle to the TEB with the most recently specified set of modifiers */
				teb[link_cnt].db_ptr = &ib_link[link_cnt]->handle.db;
				teb[link_cnt].tpb_len = tpb_len;
				teb[link_cnt].tpb_ptr = &tpb[TPB_MAX_SIZE * link_cnt];

				++link_cnt;

			} else {

				tpb_len = 0;

				convert_to_long_ex(&args[i]);
				trans_argl = Z_LVAL(args[i]);
				if (trans_argl != PHP_IBASE_DEFAULT) {
					// Skip conflicting parameters
					if (PHP_IBASE_NOWAIT != (trans_argl & PHP_IBASE_NOWAIT) && PHP_IBASE_WAIT == (trans_argl & PHP_IBASE_WAIT)) {
						if (PHP_IBASE_LOCK_TIMEOUT == (trans_argl & PHP_IBASE_LOCK_TIMEOUT)) {
							if((i + 1 < argn) && (Z_TYPE(args[i + 1]) == IS_LONG)){
								i++;
								convert_to_long_ex(&args[i]);
								trans_timeout = Z_LVAL(args[i]);
							} else {
								php_error_docref(NULL, E_WARNING, "IBASE_LOCK_TIMEOUT expects next argument to be timeout value");
							}
						}
					}
					_php_fbird_populate_trans(trans_argl, trans_timeout, last_tpb, &tpb_len);
				}
			}
		}

		if (link_cnt > 0) {
			result = isc_start_multiple(IB_STATUS, (isc_tr_handle*)&tr_handle, link_cnt, teb);
		}

		efree(tpb);
		efree(teb);
	}

	if (link_cnt == 0) {
		link_cnt = 1;
		if ((ib_link[0] = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), LE_LINK, le_link, le_plink)) == NULL) {
			efree(ib_link);
			RETURN_FALSE;
		}
		result = isc_start_transaction(IB_STATUS, (isc_tr_handle*)&tr_handle, 1, &ib_link[0]->handle.db, tpb_len, last_tpb);
	}

	/* start the transaction */
	/* cppcheck-suppress uninitvar */
	if (result) {
		_php_fbird_error();
		efree(ib_link);
		RETURN_FALSE;
	}

	/* register the transaction in our own data structures */
	ib_trans = (fbird_transaction *) safe_emalloc(link_cnt-1, sizeof(fbird_db_link *), sizeof(fbird_transaction));
	ib_trans->handle.ptr = tr_handle;
	ib_trans->link_cnt = link_cnt;
	ib_trans->affected_rows = 0;
	ib_trans->fbt_transaction = NULL;  /* Phase 4: Initialize OO API transaction pointer */
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
	RETVAL_RES(zend_register_resource(ib_trans, le_trans));
	Z_TRY_ADDREF_P(return_value);
}
/* }}} */

int _php_fbird_def_trans(fbird_db_link *ib_link, fbird_transaction **trans) /* {{{ */
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
			tr->handle.ptr = 0;
			tr->link_cnt = 1;
			tr->affected_rows = 0;
			tr->fbt_transaction = NULL;
			tr->db_link[0] = ib_link;
			ib_link->tr_list->trans = tr;
		}
		if (tr->handle.ptr == 0) {
			zend_long trans_argl = IBG(default_trans_params);
			char last_tpb[TPB_MAX_SIZE];
			unsigned short tpb_len = 0;

			/* Build TPB if non-default parameters */
			if (trans_argl != PHP_IBASE_DEFAULT) {
				zend_long trans_timeout = IBG(default_lock_timeout);
				_php_fbird_populate_trans(trans_argl, trans_timeout, last_tpb, &tpb_len);
			}

			/* Phase 4: Use OO API transaction when connection was created via OO API */
			if (ib_link->fbc_connection != NULL) {
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

				/* Store a compatible handle for legacy code paths that may inspect it */
				tr->handle.ptr = fbt_get_handle(tr->fbt_transaction);
			} else {
				/* Legacy path: use isc_start_transaction */
				ISC_STATUS result;
				if (trans_argl == PHP_IBASE_DEFAULT) {
					result = isc_start_transaction(IB_STATUS, &tr->handle.tr, 1, &ib_link->handle.db, 0, NULL);
				} else {
					result = isc_start_transaction(IB_STATUS, &tr->handle.tr, 1, &ib_link->handle.db, tpb_len, last_tpb);
				}

				if (result) {
					_php_fbird_error();
					return FAILURE;
				}
			}
		}
		*trans = tr;
	}
	return SUCCESS;
}
/* }}} */

static void _php_fbird_trans_end(INTERNAL_FUNCTION_PARAMETERS, int commit) /* {{{ */
{
	fbird_transaction *trans = NULL;
	int res_id = 0;
	ISC_STATUS result;
	fbird_db_link *ib_link;
	zval *arg = NULL;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r", &arg) == FAILURE) {
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
		/* one id was passed, could be db or trans id */
		if (Z_RES_P(arg)->type == le_trans) {
			trans = (fbird_transaction *)zend_fetch_resource_ex(arg, LE_TRANS, le_trans);
			res_id = Z_RES_P(arg)->handle;
		} else {
			ib_link = (fbird_db_link *)zend_fetch_resource2_ex(arg, LE_LINK, le_link, le_plink);

			if (ib_link->tr_list == NULL || ib_link->tr_list->trans == NULL) {
				/* this link doesn't have a default transaction */
				_php_fbird_module_error("Link has no default transaction");
				RETURN_FALSE;
			}
			trans = ib_link->tr_list->trans;
		}
	}

	/* Phase 4: Use OO API transaction end when transaction was created via OO API */
	if (trans->fbt_transaction != NULL) {
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

		if (result) {
			_php_fbird_error();
			RETURN_FALSE;
		}

		/* Clear handle for non-retained operations */
		if ((commit & RETAIN) == 0) {
			trans->fbt_transaction = NULL;
			trans->handle.ptr = 0;
		}
	} else {
		/* Legacy path: use isc_* functions */
		switch (commit) {
			default: /* == case ROLLBACK: */
				result = isc_rollback_transaction(IB_STATUS, &trans->handle.tr);
				break;
			case COMMIT:
				result = isc_commit_transaction(IB_STATUS, &trans->handle.tr);
				break;
			case (ROLLBACK | RETAIN):
				result = isc_rollback_retaining(IB_STATUS, &trans->handle.tr);
				break;
			case (COMMIT | RETAIN):
				result = isc_commit_retaining(IB_STATUS, &trans->handle.tr);
				break;
		}

		if (result) {
			_php_fbird_error();
			RETURN_FALSE;
		}
	}

	/* Don't try to destroy implicitly opened transaction from list... */
	if ((commit & RETAIN) == 0 && res_id != 0) {
		zend_list_delete(Z_RES_P(arg));
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto fbird_commit( resource link_identifier )
   Commit transaction */
PHP_FUNCTION(fbird_commit)
{
	_php_fbird_trans_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, COMMIT);
}
/* }}} */

/* {{{ proto fbird_rollback( resource link_identifier )
   Rollback transaction */
PHP_FUNCTION(fbird_rollback)
{
	_php_fbird_trans_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, ROLLBACK);
}
/* }}} */

/* {{{ proto fbird_commit_ret( resource link_identifier )
   Commit transaction and retain the transaction context */
PHP_FUNCTION(fbird_commit_ret)
{
	_php_fbird_trans_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, COMMIT | RETAIN);
}
/* }}} */

/* {{{ proto fbird_rollback_ret( resource link_identifier )
   Rollback transaction and retain the transaction context */
PHP_FUNCTION(fbird_rollback_ret)
{
	_php_fbird_trans_end(INTERNAL_FUNCTION_PARAM_PASSTHRU, ROLLBACK | RETAIN);
}
/* }}} */

static int is_valid_identifier(const char *s, size_t len)
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

/* {{{ proto fbird_gen_id(string generator [, int increment [, resource link_identifier ]])
   Increments the named generator and returns its new value */
PHP_FUNCTION(fbird_gen_id)
{
	zval *link = NULL;
	char query[128], *generator;
	size_t gen_len;
	zend_long inc = 1;
	fbird_db_link *ib_link = NULL;
	fbird_transaction *trans = NULL;
	XSQLDA out_sqlda;
	ISC_INT64 result = 0;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "s|lr", &generator, &gen_len,
			&inc, &link)) {
		RETURN_FALSE;
	}

	if (gen_len > 31) {
		php_error_docref(NULL, E_WARNING, "Invalid generator name (length > 31 characters)");
		RETURN_FALSE;
	}

	if (!is_valid_identifier(generator, gen_len)) {
		php_error_docref(NULL, E_WARNING, "Invalid generator name (contains invalid characters)");
		RETURN_FALSE;
	}

	PHP_IBASE_LINK_TRANS(link, ib_link, trans);

	snprintf(query, sizeof(query), "SELECT GEN_ID(%s,%ld) FROM rdb$database", generator, inc);

	/* allocate a minimal descriptor area */
	out_sqlda.sqln = out_sqlda.sqld = 1;
	out_sqlda.version = SQLDA_CURRENT_VERSION;

	/* allocate the field for the result */
	out_sqlda.sqlvar[0].sqltype = SQL_INT64;
	out_sqlda.sqlvar[0].sqlscale = 0;
	out_sqlda.sqlvar[0].sqllen = sizeof(result);
	out_sqlda.sqlvar[0].sqldata = (void*) &result;

	/* execute the query */
	if (isc_dsql_exec_immed2(IB_STATUS, &ib_link->handle.db, &trans->handle.tr, 0, query,
			SQL_DIALECT_CURRENT, NULL, &out_sqlda)) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* don't return the generator value as a string unless it doesn't fit in a long */
#if SIZEOF_ZEND_LONG < 8
	if (result < ZEND_LONG_MIN || result > ZEND_LONG_MAX) {
		char *res;
		int l;

		l = spprintf(&res, 0, "%" LL_MASK "d", result);
		RETURN_STRINGL(res, l);
	}
#endif
	RETURN_LONG((zend_long)result);
}

#if PHP_DEBUG
void fbp_dump_buffer(int len, const unsigned char *buffer)
{
	int i;
	for (i = 0; i < len; i++) {
		if(buffer[i] < 32 || buffer[i] > 126)
			php_printf("0x%02x ", buffer[i]);
		else
			php_printf(" [%c] ", buffer[i]);
		if(i % 16 == 15)php_printf("\n");
	}
	if(i > 0)php_printf("\n");
}

void fbp_dump_buffer_raw(int len, const unsigned char *buffer)
{
	int i;
	for (i = 0; i < len; i++) {
		php_printf("%c", buffer[i]);
	}
}
#endif

void fbp_error_ex(long level, const char *msg, ...)
{
	va_list ap;
	char buf[1024] = {0};

	va_start(ap, msg);

	/* vsnprintf NUL terminates the buf and writes at most n-1 chars+NUL */
	vsnprintf(buf, sizeof(buf), msg, ap);
	va_end(ap);

	// IBG(sql_code) = -999; /* no SQL error */

	php_error(level, "%s", buf);
}

/* }}} */


#endif /* HAVE_FIREBIRD */

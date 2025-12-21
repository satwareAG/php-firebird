/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

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
#include "php_fbird_query_internal.h"
#include "php_fbird_inspection.h"
#include "SAPI.h"
#include "zend_exceptions.h"
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include "firebird_utils.h"
#include "fbird_datetime.h"

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

ZEND_BEGIN_ARG_INFO(arginfo_fbird_sqlstate, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_connect, 0, 0, 0)
	ZEND_ARG_INFO(0, database)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, charset)
	ZEND_ARG_INFO(0, buffers)
	ZEND_ARG_INFO(0, dialect)
	ZEND_ARG_INFO(0, role)
	ZEND_ARG_INFO(0, flags)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_pconnect, 0, 0, 0)
	ZEND_ARG_INFO(0, database)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, charset)
	ZEND_ARG_INFO(0, buffers)
	ZEND_ARG_INFO(0, dialect)
	ZEND_ARG_INFO(0, role)
	ZEND_ARG_INFO(0, flags)
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_create_seekable, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_open_seekable, 0, 0, 1)
	ZEND_ARG_INFO(0, blob_id)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_seek, 0, 0, 2)
	ZEND_ARG_INFO(0, blob_handle)
	ZEND_ARG_INFO(0, offset)
	ZEND_ARG_INFO(0, whence)
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_connection_info, 0, 0, 0)
	ZEND_ARG_TYPE_INFO(0, link_identifier, IS_RESOURCE, 1)
ZEND_END_ARG_INFO()

/* Limbo Transaction Functions (Two-Phase Commit Recovery) */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_get_limbo_transactions, 0, 0, 0)
	ZEND_ARG_TYPE_INFO(0, link_identifier, IS_RESOURCE, 1)
	ZEND_ARG_TYPE_INFO(0, max_count, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_reconnect_transaction, 0, 0, 2)
	ZEND_ARG_TYPE_INFO(0, link_identifier, IS_RESOURCE, 0)
	ZEND_ARG_TYPE_INFO(0, transaction_id, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* IBatch API Functions (Firebird 4.0+ Bulk Operations) */
#if FB_API_VER >= 40
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_create, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, query, IS_RESOURCE, 0)
	ZEND_ARG_TYPE_INFO(0, trans_identifier, IS_RESOURCE, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_add, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, batch, IS_RESOURCE, 0)
	ZEND_ARG_VARIADIC_INFO(0, bind_args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_execute, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, batch, IS_RESOURCE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_cancel, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, batch, IS_RESOURCE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_add_blob, 0, 0, 2)
	ZEND_ARG_TYPE_INFO(0, batch, IS_RESOURCE, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, blob_type, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_register_blob, 0, 0, 2)
	ZEND_ARG_TYPE_INFO(0, batch, IS_RESOURCE, 0)
	ZEND_ARG_TYPE_INFO(0, blob_id, IS_STRING, 0)
ZEND_END_ARG_INFO()
#endif /* FB_API_VER >= 40 */
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
	PHP_FE(fbird_blob_create_seekable, arginfo_fbird_blob_create_seekable)
	PHP_FE(fbird_blob_open_seekable, arginfo_fbird_blob_open_seekable)
	PHP_FE(fbird_blob_seek, 	arginfo_fbird_blob_seek)
	PHP_FE(fbird_errmsg, 		arginfo_fbird_errmsg)
	PHP_FE(fbird_errcode, 		arginfo_fbird_errcode)
	PHP_FE(fbird_sqlstate, 		arginfo_fbird_sqlstate)

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

	PHP_FE(fbird_trans_start,		arginfo_fbird_trans_start)
	PHP_FE(fbird_savepoint,			arginfo_fbird_savepoint)
	PHP_FE(fbird_rollback_savepoint,	arginfo_fbird_savepoint)
	PHP_FE(fbird_release_savepoint,	arginfo_fbird_savepoint)
	PHP_FE(fbird_trans_info,		arginfo_fbird_trans_info)

	PHP_FE(fbird_connection_info,	arginfo_fbird_connection_info)

	/* Limbo Transaction Functions (Two-Phase Commit Recovery) */
	PHP_FE(fbird_get_limbo_transactions, arginfo_fbird_get_limbo_transactions)
	PHP_FE(fbird_reconnect_transaction, arginfo_fbird_reconnect_transaction)

#if FB_API_VER >= 40
	/* IBatch API Functions (Firebird 4.0+ Bulk Operations) */
	PHP_FE(fbird_batch_create, arginfo_fbird_batch_create)
	PHP_FE(fbird_batch_add, arginfo_fbird_batch_add)
	PHP_FE(fbird_batch_execute, arginfo_fbird_batch_execute)
	PHP_FE(fbird_batch_cancel, arginfo_fbird_batch_cancel)
	PHP_FE(fbird_batch_add_blob, arginfo_fbird_batch_add_blob)
	PHP_FE(fbird_batch_register_blob, arginfo_fbird_batch_register_blob)
#endif /* FB_API_VER >= 40 */

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
#if FB_API_VER >= 40
int le_batch;
#endif

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

/* {{{ proto fbird_sqlstate(void)
   Return SQLSTATE error code for the last error */
PHP_FUNCTION(fbird_sqlstate)
{
	char sqlstate[6]; /* 5 chars + null terminator */

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	/* Check if there is an error to report */
	if (IBG(sql_code) == 0) {
		RETURN_FALSE;
	}

	/* Call fb_sqlstate to get the SQLSTATE code from the status vector */
	fb_sqlstate(sqlstate, IB_STATUS);

	/* fb_sqlstate always returns a 5-character string, with "00000" for success */
	if (sqlstate[0] == '0' && sqlstate[1] == '0' && sqlstate[2] == '0' &&
	    sqlstate[3] == '0' && sqlstate[4] == '0') {
		/* No error state */
		RETURN_FALSE;
	}

	RETURN_STRINGL(sqlstate, 5);
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
	FBDEBUG("Transaction or database link?");
	if (Z_RES_P(link_id)->type == le_trans) {
		/* Transaction resource: make sure it refers to one link only, then
		   fetch it; database link is stored in ib_trans->db_link[]. */
		FBDEBUG("Type is le_trans");
		*trans = (fbird_transaction *)zend_fetch_resource_ex(link_id, LE_TRANS, le_trans);
		if ((*trans)->link_cnt > 1) {
			_php_fbird_module_error("Link id is ambiguous: transaction spans multiple connections."
				);
			return;
		}
		*ib_link = (*trans)->db_link[0];
		return;
	}
	FBDEBUG("Type is le_[p]link or id not found");
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
	FBDEBUG("Checking transactions to close...");

	for (l = link->tr_list; l != NULL; ++i) {
		fbird_tr_list *p = l;
		if (p->trans != 0) {
			if (i == 0) {
				/* Default transaction: commit via OO API */
				if (p->trans->fbt_transaction != NULL) {
					FBDEBUG("Committing default transaction via OO API...");
					if (fbt_commit(p->trans->fbt_transaction, IB_STATUS)) {
						_php_fbird_error();
					}
					p->trans->fbt_transaction = NULL;
				}
				efree(p->trans); /* default transaction is not a registered resource: clean up */
			} else {
				/* Non-default transaction: rollback via OO API */
				if (p->trans->fbt_transaction != NULL) {
					FBDEBUG("Rolling back other transaction via OO API...");
					if (fbt_rollback(p->trans->fbt_transaction, IB_STATUS)) {
						_php_fbird_error();
					}
					p->trans->fbt_transaction = NULL;
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

	/* OO API Only: All connections use fbc_disconnect() */
	if (link->fbc_connection != NULL) {
		FBDEBUG("Closing normal link via OO API...");
		fbc_disconnect(link->fbc_connection, IB_STATUS);
		link->fbc_connection = NULL;
		link->handle.ptr = 0;
	}
	IBG(num_links)--;
	efree(link);
}
/* }}} */

static void _php_fbird_close_plink(zend_resource *rsrc) /* {{{ */
{
	fbird_db_link *link = (fbird_db_link *) rsrc->ptr;

	_php_fbird_commit_link(link);

	/* OO API Only: All connections use fbc_disconnect() */
	if (link->fbc_connection != NULL) {
		FBDEBUG("Closing permanent link via OO API...");
		fbc_disconnect(link->fbc_connection, IB_STATUS);
		link->fbc_connection = NULL;
		link->handle.ptr = 0;
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

	FBDEBUG("Cleaning up transaction resource...");

	/* OO API Only: All transactions use fbt_rollback() */
	if (trans->fbt_transaction != NULL) {
		FBDEBUG("Rolling back unhandled OO API transaction...");
		if (fbt_rollback(trans->fbt_transaction, IB_STATUS)) {
			_php_fbird_error();
		}
		trans->fbt_transaction = NULL;
		trans->handle.ptr = 0;
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

#if FB_API_VER >= 40
static void _php_fbird_free_batch(zend_resource *rsrc) /* {{{ */
{
	fbird_batch *batch = (fbird_batch *)rsrc->ptr;

	FBDEBUG("Cleaning up batch resource...");

	/* Cancel and close the batch if still open */
	if (batch->fbbatch_wrapper != NULL) {
		FBDEBUG("Canceling unexecuted batch...");
		fbbatch_cancel(IBG(master_instance), batch->fbbatch_wrapper, IB_STATUS);
		fbbatch_close(IBG(master_instance), batch->fbbatch_wrapper, IB_STATUS);
		batch->fbbatch_wrapper = NULL;
	}

	/* Free the input message buffer if allocated */
	if (batch->in_msg_buffer != NULL) {
		efree(batch->in_msg_buffer);
		batch->in_msg_buffer = NULL;
	}

	/* Release metadata reference if held */
	if (batch->in_metadata != NULL) {
		fbm_release(batch->in_metadata);
		batch->in_metadata = NULL;
	}

	efree(batch);
}
/* }}} */
#endif /* FB_API_VER >= 40 */

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

		if (trans_argl != PHP_FBIRD_DEFAULT) {
			/* access mode */
			if (PHP_FBIRD_READ == (trans_argl & PHP_FBIRD_READ)) {
				PUTS_TP("FBIRD_READ");
			} else if (PHP_FBIRD_WRITE == (trans_argl & PHP_FBIRD_WRITE)) {
				PUTS_TP("FBIRD_WRITE");
			}

			/* isolation level */
			if (PHP_FBIRD_COMMITTED == (trans_argl & PHP_FBIRD_COMMITTED)) {
				PUTS_TP("FBIRD_COMMITTED");
				if (PHP_FBIRD_REC_VERSION == (trans_argl & PHP_FBIRD_REC_VERSION)) {
					PUTS_TP("FBIRD_REC_VERSION");
				} else if (PHP_FBIRD_REC_NO_VERSION == (trans_argl & PHP_FBIRD_REC_NO_VERSION)) {
					PUTS_TP("FBIRD_REC_NO_VERSION");
				}
			} else if (PHP_FBIRD_CONSISTENCY == (trans_argl & PHP_FBIRD_CONSISTENCY)) {
				PUTS_TP("FBIRD_CONSISTENCY");
			} else if (PHP_FBIRD_CONCURRENCY == (trans_argl & PHP_FBIRD_CONCURRENCY)) {
				PUTS_TP("FBIRD_CONCURRENCY");
			}

			/* lock resolution */
			if (PHP_FBIRD_NOWAIT == (trans_argl & PHP_FBIRD_NOWAIT)) {
				PUTS_TP("FBIRD_NOWAIT");
			} else if (PHP_FBIRD_WAIT == (trans_argl & PHP_FBIRD_WAIT)) {
				PUTS_TP("FBIRD_WAIT");
				if (PHP_FBIRD_LOCK_TIMEOUT == (trans_argl & PHP_FBIRD_LOCK_TIMEOUT)) {
					PUTS_TP("FBIRD_LOCK_TIMEOUT");
				}
			}
		} else {
			PUTS_TP("FBIRD_DEFAULT");
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
	REGISTER_LONG_CONSTANT("FBIRD_DEFAULT", PHP_FBIRD_DEFAULT, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_CREATE", PHP_FBIRD_CREATE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_TEXT", PHP_FBIRD_FETCH_BLOBS, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_FETCH_BLOBS", PHP_FBIRD_FETCH_BLOBS, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_FETCH_ARRAYS", PHP_FBIRD_FETCH_ARRAYS, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_UNIXTIME", PHP_FBIRD_UNIXTIME, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_FETCH_DATE_OBJ", PHP_FBIRD_FETCH_DATE_OBJ, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_VER", PHP_FIREBIRD_VER, CONST_PERSISTENT);

	/* transactions */
	REGISTER_LONG_CONSTANT("FBIRD_WRITE", PHP_FBIRD_WRITE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_READ", PHP_FBIRD_READ, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_COMMITTED", PHP_FBIRD_COMMITTED, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_CONSISTENCY", PHP_FBIRD_CONSISTENCY, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_CONCURRENCY", PHP_FBIRD_CONCURRENCY, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_REC_VERSION", PHP_FBIRD_REC_VERSION, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_REC_NO_VERSION", PHP_FBIRD_REC_NO_VERSION, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_NOWAIT", PHP_FBIRD_NOWAIT, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_WAIT", PHP_FBIRD_WAIT, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_TIMEOUT", PHP_FBIRD_LOCK_TIMEOUT, CONST_PERSISTENT);

	/* Table reservation constants */
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_SHARED", PHP_FBIRD_LOCK_SHARED, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_PROTECTED", PHP_FBIRD_LOCK_PROTECTED, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_EXCLUSIVE", PHP_FBIRD_LOCK_EXCLUSIVE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_READ", PHP_FBIRD_LOCK_READ, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_LOCK_WRITE", PHP_FBIRD_LOCK_WRITE, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_READ_CONSISTENCY", PHP_FBIRD_READ_CONSISTENCY, CONST_PERSISTENT);

	/* Event constants */
	REGISTER_LONG_CONSTANT("FBIRD_EVENT_TIMEOUT", PHP_FBIRD_EVENT_TIMEOUT, CONST_PERSISTENT);

	/* Connection flags (matches PostgreSQL PGSQL_CONNECT_FORCE_NEW) */
	REGISTER_LONG_CONSTANT("FBIRD_CONNECT_FORCE_NEW", PHP_FBIRD_CONNECT_FORCE_NEW, CONST_PERSISTENT);

	/* BLOB seek constants (mirrors SEEK_SET, SEEK_CUR, SEEK_END) */
	REGISTER_LONG_CONSTANT("FBIRD_BLOB_SEEK_SET", 0, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_BLOB_SEEK_CUR", 1, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_BLOB_SEEK_END", 2, CONST_PERSISTENT);

#if FB_API_VER >= 40
	le_batch = zend_register_list_destructors_ex(_php_fbird_free_batch, NULL, LE_BATCH, module_number);
#endif

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
	/*
	 * Firebird client library registers an atexit() handler for cleanup.
	 * NULL the dlopen() handle to prevent dlclose() from being called,
	 * avoiding segfaults during module unload.
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

	php_info_print_table_row(2, "Firebird extension version", PHP_FIREBIRD_VER_STR);

#ifdef FB_API_VER
	snprintf( (s = tmp), sizeof(tmp), "Firebird API version %d", FB_API_VER);
#elif (SQLDA_CURRENT_VERSION > 1)
	s =  "Firebird 3.0 and up";
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

    /* Store the OO API connection pointer in the status vector's last slot
     * for retrieval by _php_fbird_connect() */
    IBG(status[ISC_STATUS_LENGTH - 1]) = (ISC_STATUS)(uintptr_t)connection;

    *db = NULL;

    return SUCCESS;
}
/* }}} */

static void _php_fbird_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent) /* {{{ */
{
	char *c, hash[16], *args[] = { NULL, NULL, NULL, NULL, NULL };
	int i;
	size_t len[] = { 0, 0, 0, 0, 0 };
	zend_long largs[] = { 0, 0, 0 };
	zend_long flags = 0;
	PHP_MD5_CTX hash_context;
	zend_resource new_index_ptr, *le;
	void *db_handle = 0;
	fbird_db_link *ib_link;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|ssssllsll",
			&args[DB], &len[DB], &args[USER], &len[USER], &args[PASS], &len[PASS],
			&args[CSET], &len[CSET], &largs[BUF], &largs[DLECT], &args[ROLE], &len[ROLE],
			&largs[SYNC], &flags)) {
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

	/* try to reuse a connection (skip if FBIRD_CONNECT_FORCE_NEW is set) */
	if (!(flags & PHP_FBIRD_CONNECT_FORCE_NEW) &&
			(le = zend_hash_str_find_ptr(&EG(regular_list), hash, sizeof(hash)-1)) != NULL) {
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
   Open a connection to a Firebird database */
PHP_FUNCTION(fbird_connect)
{
	_php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto fbird_pconnect([string database [, string username [, string password [, string charset [, int buffers [, int dialect [, string role]]]]]]])
   Open a persistent connection to a Firebird database */
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
   Close a Firebird connection */
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
   Drop a Firebird database */
PHP_FUNCTION(fbird_drop_db)
{
	zval *link_arg = NULL;
	fbird_db_link *ib_link;
	fbird_tr_list *l;
	zend_resource *link_res;
	int drop_result;

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
		ib_link->handle.ptr = 0;
	}

	/* drop_database() doesn't invalidate the transaction handles */
	for (l = ib_link->tr_list; l != NULL; l = l->next) {
		if (l->trans != NULL) {
			l->trans->handle.ptr = 0;
			l->trans->fbt_transaction = NULL;
		}
	}

	zend_list_delete(link_res);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto resource fbird_transaction([int trans_args [, resource link_identifier [, ... ], int trans_args [, resource link_identifier [, ... ]] [, ...]]])
   Start a transaction */

#define TPB_MAX_SIZE 2048

void _php_fbird_populate_trans(zend_long trans_argl, zend_long trans_timeout, char *last_tpb, unsigned short *len) /* {{{ */
{
	/* No explicit flags: leave TPB empty so Firebird uses its defaults. */
	if (trans_argl == PHP_FBIRD_DEFAULT) {
		*len = 0;
		return;
	}

	/*
	 * Use IXpbBuilder-based TPB construction via OO API.
	 * This replaces manual byte array construction with the modern
	 * Firebird 3.0+ builder pattern for cleaner, type-safe TPB generation.
	 *
	 * See: fbxpb_build_tpb() in firebird_utils.cpp
	 */
	unsigned int buffer_length = 0;
	ISC_STATUS local_status[ISC_STATUS_LENGTH] = {0};

	unsigned char *tpb_buffer = fbxpb_build_tpb(
		IBG(master_instance),
		trans_argl,
		trans_timeout,
		&buffer_length,
		local_status
	);

	if (tpb_buffer == NULL) {
		/* Fallback: if OO API fails, report error and return empty TPB */
		php_error_docref(NULL, E_WARNING, "DEBUG: fbxpb_build_tpb returned NULL, local_status[1]=%ld", (long)local_status[1]);
		*len = 0;
		return;
	}

	/* Copy to caller's buffer (limited by TPB_MAX_SIZE) */
	if (buffer_length > TPB_MAX_SIZE) {
		buffer_length = TPB_MAX_SIZE;
	}
	memcpy(last_tpb, tpb_buffer, buffer_length);
	*len = (unsigned short) buffer_length;

	/* Free the OO API allocated buffer */
	fbxpb_free_tpb(tpb_buffer);
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
			if (Z_LVAL_P(tmp) & PHP_FBIRD_READ) {
				*p++ = isc_tpb_read;
			} else if (Z_LVAL_P(tmp) & PHP_FBIRD_WRITE) {
				*p++ = isc_tpb_write;
			}
		}
	}

	/* isolation level */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "isolation", sizeof("isolation") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			zend_long iso = Z_LVAL_P(tmp);
			if (iso & PHP_FBIRD_COMMITTED) {
				*p++ = isc_tpb_read_committed;
				if (iso & PHP_FBIRD_REC_VERSION) {
					*p++ = isc_tpb_rec_version;
				} else if (iso & PHP_FBIRD_REC_NO_VERSION) {
					*p++ = isc_tpb_no_rec_version;
				}
			} else if (iso & PHP_FBIRD_CONSISTENCY) {
				*p++ = isc_tpb_consistency;
			} else if (iso & PHP_FBIRD_CONCURRENCY) {
				*p++ = isc_tpb_concurrency;
			}
		}
	}

	/* lock resolution */
	if ((tmp = zend_hash_str_find(Z_ARRVAL_P(options), "lock_resolution", sizeof("lock_resolution") - 1)) != NULL) {
		if (Z_TYPE_P(tmp) == IS_LONG) {
			zend_long res = Z_LVAL_P(tmp);
			if (res & PHP_FBIRD_NOWAIT) {
				*p++ = isc_tpb_nowait;
			} else if (res & PHP_FBIRD_WAIT) {
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
					if (lock_mode & PHP_FBIRD_LOCK_WRITE) {
						if (p < end) *p++ = isc_tpb_lock_write;
					} else if (lock_mode & PHP_FBIRD_LOCK_READ) {
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
					if (lock_mode & PHP_FBIRD_LOCK_PROTECTED) {
						if (p < end) *p++ = isc_tpb_protected;
					} else if (lock_mode & PHP_FBIRD_LOCK_EXCLUSIVE) {
						if (p < end) *p++ = isc_tpb_exclusive;
					} else if (lock_mode & PHP_FBIRD_LOCK_SHARED) {
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

	/* OO API Only: All connections use fbt_start() */
	if (ib_link->fbc_connection == NULL) {
		_php_fbird_module_error("Connection has no OO API handle");
		RETURN_FALSE;
	}

	void* attachment = fbc_get_attachment(ib_link->fbc_connection);
	if (attachment == NULL) {
		_php_fbird_module_error("Failed to get attachment from OO API connection");
		RETURN_FALSE;
	}

	ib_trans = (fbird_transaction *) safe_emalloc(1-1, sizeof(fbird_db_link *), sizeof(fbird_transaction));
	ib_trans->fbt_transaction = fbt_start(
		IBG(master_instance),
		attachment,
		tpb_len,
		tpb_len > 0 ? (const unsigned char*)last_tpb : NULL,
		IB_STATUS
	);

	if (ib_trans->fbt_transaction == NULL) {
		efree(ib_trans);
		_php_fbird_error();
		RETURN_FALSE;
	}

	ib_trans->handle.ptr = fbt_get_handle(ib_trans->fbt_transaction);
	ib_trans->link_cnt = 1;
	ib_trans->affected_rows = 0;
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

	/* OO API Only: All transactions use fbs_prepare() + fbs_execute() */
	if (trans->fbt_transaction == NULL) {
		_php_fbird_module_error("Transaction has no OO API handle");
		efree(query);
		RETURN_FALSE;
	}

	fbird_db_link *link = trans->db_link[0];
	void *attachment = NULL;
	void *transaction_ptr = NULL;
	void *stmt = NULL;

	FBDEBUG("OO API: Executing savepoint statement via fbs_prepare/execute");

	/* Get attachment from connection */
	if (!link->fbc_connection) {
		_php_fbird_module_error("OO API transaction without OO API connection");
		efree(query);
		RETURN_FALSE;
	}
	attachment = fbc_get_attachment(link->fbc_connection);
	if (!attachment) {
		_php_fbird_module_error("Failed to get attachment from connection");
		efree(query);
		RETURN_FALSE;
	}

	/* Get transaction handle */
	transaction_ptr = fbt_get_handle(trans->fbt_transaction);
	if (!transaction_ptr) {
		_php_fbird_module_error("Failed to get transaction handle");
		efree(query);
		RETURN_FALSE;
	}

	/* Prepare the savepoint statement */
	stmt = fbs_prepare(IBG(master_instance), attachment, transaction_ptr,
		query, (unsigned)len, SQL_DIALECT_CURRENT, IB_STATUS);
	if (!stmt) {
		_php_fbird_error();
		efree(query);
		RETURN_FALSE;
	}

	/* Execute the savepoint statement (no input/output parameters) */
	if (!fbs_execute(IBG(master_instance), stmt, transaction_ptr,
			NULL, NULL, NULL, NULL, IB_STATUS)) {
		_php_fbird_error();
		fbs_free(stmt, IB_STATUS);
		efree(query);
		RETURN_FALSE;
	}

	/* Free the statement */
	fbs_free(stmt, IB_STATUS);
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

	/*
	 * Firebird 3.0+ OO API
	 *
	 * OO API transactions do not have a valid legacy isc_tr_handle.
	 * For OO API transactions, trans->handle.ptr stores the raw ITransaction*
	 * (from fbt_get_handle()).
	 */
	if (trans->fbt_transaction != NULL) {
		if (trans->handle.ptr == NULL) {
			_php_fbird_module_error("Transaction has no valid OO API handle");
			RETURN_FALSE;
		}

		if (fbt_get_info(
				IBG(master_instance),
				trans->handle.ptr,
				sizeof(tpb),
				(const unsigned char*)tpb,
				sizeof(res_buf),
				(unsigned char*)res_buf,
				IB_STATUS
			) == 0) {
			_php_fbird_error();
			RETURN_FALSE;
		}
	} else {
		_php_fbird_module_error("Transaction has no OO API handle");
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

/* {{{ proto array fbird_connection_info([resource link_identifier])
   Return database connection statistics and information */
PHP_FUNCTION(fbird_connection_info)
{
	zval *link_arg = NULL;
	fbird_db_link *ib_link;
	char info_items[] = {
		isc_info_reads,
		isc_info_writes,
		isc_info_fetches,
		isc_info_marks,
		isc_info_page_size,
		isc_info_num_buffers,
		isc_info_current_memory,
		isc_info_max_memory,
		isc_info_allocation,
		isc_info_attachment_id,
		isc_info_ods_version,
		isc_info_ods_minor_version,
		isc_info_db_sql_dialect,
		isc_info_end
	};
	char res_buf[512];
	char *p;
	ISC_STATUS status[ISC_STATUS_LENGTH];

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r!", &link_arg) == FAILURE) {
		return;
	}

	if (link_arg == NULL) {
		ib_link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), LE_LINK, le_link, le_plink);
	} else {
		ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
	}

	if (!ib_link) {
		RETURN_FALSE;
	}

	/* Use OO API for connections that have fbc_connection */
	if (ib_link->fbc_connection != NULL) {
		void* attachment = fbc_get_attachment(ib_link->fbc_connection);
		if (attachment == NULL) {
			_php_fbird_module_error("Failed to get attachment from OO API connection");
			RETURN_FALSE;
		}

		if (!fbc_get_info(IBG(master_instance), attachment,
				sizeof(info_items), (const unsigned char*)info_items,
				sizeof(res_buf), (unsigned char*)res_buf, IB_STATUS)) {
			_php_fbird_error();
			RETURN_FALSE;
		}
	} else {
		/* Fallback to legacy API for connections without OO API handle */
		if (isc_database_info(status, &ib_link->handle.db, sizeof(info_items), info_items,
				sizeof(res_buf), res_buf)) {
			memcpy(IB_STATUS, status, sizeof(status));
			_php_fbird_error();
			RETURN_FALSE;
		}
	}

	array_init(return_value);
	p = res_buf;

	while (*p != isc_info_end && p < res_buf + sizeof(res_buf)) {
		unsigned char item = *p++;
		unsigned short len = (unsigned short)isc_vax_integer(p, 2);
		p += 2;

		switch (item) {
			case isc_info_reads:
				add_assoc_long(return_value, "reads", isc_vax_integer(p, len));
				break;
			case isc_info_writes:
				add_assoc_long(return_value, "writes", isc_vax_integer(p, len));
				break;
			case isc_info_fetches:
				add_assoc_long(return_value, "fetches", isc_vax_integer(p, len));
				break;
			case isc_info_marks:
				add_assoc_long(return_value, "marks", isc_vax_integer(p, len));
				break;
			case isc_info_page_size:
				add_assoc_long(return_value, "page_size", isc_vax_integer(p, len));
				break;
			case isc_info_num_buffers:
				add_assoc_long(return_value, "num_buffers", isc_vax_integer(p, len));
				break;
			case isc_info_current_memory:
				add_assoc_long(return_value, "current_memory", isc_vax_integer(p, len));
				break;
			case isc_info_max_memory:
				add_assoc_long(return_value, "max_memory", isc_vax_integer(p, len));
				break;
			case isc_info_allocation:
				add_assoc_long(return_value, "allocation", isc_vax_integer(p, len));
				break;
			case isc_info_attachment_id:
				add_assoc_long(return_value, "attachment_id", isc_vax_integer(p, len));
				break;
			case isc_info_ods_version:
				add_assoc_long(return_value, "ods_version", isc_vax_integer(p, len));
				break;
			case isc_info_ods_minor_version:
				add_assoc_long(return_value, "ods_minor_version", isc_vax_integer(p, len));
				break;
			case isc_info_db_sql_dialect:
				add_assoc_long(return_value, "sql_dialect", isc_vax_integer(p, len));
				break;
			case isc_info_truncated:
				add_assoc_bool(return_value, "truncated", 1);
				return;
			default:
				break;
		}
		p += len;
	}
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
				if (trans_argl != PHP_FBIRD_DEFAULT) {
					// Skip conflicting parameters
					if (PHP_FBIRD_NOWAIT != (trans_argl & PHP_FBIRD_NOWAIT) && PHP_FBIRD_WAIT == (trans_argl & PHP_FBIRD_WAIT)) {
						if (PHP_FBIRD_LOCK_TIMEOUT == (trans_argl & PHP_FBIRD_LOCK_TIMEOUT)) {
							if((i + 1 < argn) && (Z_TYPE(args[i + 1]) == IS_LONG)){
								i++;
								convert_to_long_ex(&args[i]);
								trans_timeout = Z_LVAL(args[i]);
							} else {
								php_error_docref(NULL, E_WARNING, "FBIRD_LOCK_TIMEOUT expects next argument to be timeout value");
							}
						}
					}
					_php_fbird_populate_trans(trans_argl, trans_timeout, last_tpb, &tpb_len);
				}
			}
		}

		if (link_cnt > 0) {
			/* OO API Only: Verify all connections have OO API handles */
			for (int j = 0; j < link_cnt; j++) {
				if (ib_link[j]->fbc_connection == NULL) {
					efree(tpb);
					efree(teb);
					efree(ib_link);
					_php_fbird_module_error("Connection %d has no OO API handle", j);
					RETURN_FALSE;
				}
			}

			if (link_cnt == 1) {
				/* Single OO API connection - use fbt_start */
				void* attachment = fbc_get_attachment(ib_link[0]->fbc_connection);
				if (attachment == NULL) {
					efree(tpb);
					efree(teb);
					efree(ib_link);
					_php_fbird_module_error("Failed to get attachment from OO API connection");
					RETURN_FALSE;
				}

				void* oo_trans = fbt_start(
					IBG(master_instance),
					attachment,
					teb[0].tpb_len,
					teb[0].tpb_len > 0 ? (const unsigned char*)teb[0].tpb_ptr : NULL,
					IB_STATUS
				);

				if (oo_trans == NULL) {
					efree(tpb);
					efree(teb);
					efree(ib_link);
					_php_fbird_error();
					RETURN_FALSE;
				}

				tr_handle = fbt_get_handle(oo_trans);

				/* Allocate and register transaction with OO API wrapper */
				ib_trans = (fbird_transaction *) safe_emalloc(link_cnt-1, sizeof(fbird_db_link *), sizeof(fbird_transaction));
				ib_trans->handle.ptr = tr_handle;
				ib_trans->link_cnt = link_cnt;
				ib_trans->affected_rows = 0;
				ib_trans->fbt_transaction = oo_trans;

				efree(tpb);
				efree(teb);
				goto register_trans;
			} else {
				/* Multi-database OO API transactions not yet supported */
				efree(tpb);
				efree(teb);
				efree(ib_link);
				_php_fbird_module_error("Multi-database transactions with OO API connections not yet supported");
				RETURN_FALSE;
			}
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
		/* OO API Only: All connections must have OO API handle */
		if (ib_link[0]->fbc_connection == NULL) {
			efree(ib_link);
			_php_fbird_module_error("Connection has no OO API handle");
			RETURN_FALSE;
		}

		void* attachment = fbc_get_attachment(ib_link[0]->fbc_connection);
		if (attachment == NULL) {
			efree(ib_link);
			_php_fbird_module_error("Failed to get attachment from OO API connection");
			RETURN_FALSE;
		}

		void* oo_trans = fbt_start(
			IBG(master_instance),
			attachment,
			tpb_len,
			tpb_len > 0 ? (const unsigned char*)last_tpb : NULL,
			IB_STATUS
		);
		if (oo_trans == NULL) {
			_php_fbird_error();
			efree(ib_link);
			RETURN_FALSE;
		}
		tr_handle = fbt_get_handle(oo_trans);

		/* Allocate and register transaction with OO API wrapper */
		ib_trans = (fbird_transaction *) safe_emalloc(link_cnt-1, sizeof(fbird_db_link *), sizeof(fbird_transaction));
		ib_trans->handle.ptr = tr_handle;
		ib_trans->link_cnt = link_cnt;
		ib_trans->affected_rows = 0;
		ib_trans->fbt_transaction = oo_trans;
	}

register_trans:
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
			if (trans_argl != PHP_FBIRD_DEFAULT) {
				zend_long trans_timeout = IBG(default_lock_timeout);
				_php_fbird_populate_trans(trans_argl, trans_timeout, last_tpb, &tpb_len);
			}

			/* OO API Only: All connections use fbt_start() */
			if (ib_link->fbc_connection == NULL) {
				_php_fbird_module_error("Connection has no OO API handle");
				return FAILURE;
			}

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

	/* OO API Only: All transactions use fbt_* functions */
	if (trans->fbt_transaction == NULL) {
		_php_fbird_module_error("invalid transaction handle (expecting explicit transaction start) ");
		RETURN_FALSE;
	}

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

	/* Clear handle for non-retained operations BEFORE checking result.
	 * The fbt_* functions ALWAYS delete the wrapper (even on error),
	 * so we must clear our pointer to avoid dangling references.
	 * Fixes: #9, #10 - SIGSEGV due to use-after-free of transaction wrapper
	 */
	if ((commit & RETAIN) == 0) {
		trans->fbt_transaction = NULL;
		trans->handle.ptr = 0;
	}

	if (result) {
		_php_fbird_error();
		RETURN_FALSE;
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
	ISC_INT64 result = 0;
	void *attachment = NULL;
	void *transaction_ptr = NULL;
	void *stmt = NULL;

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

	PHP_FBIRD_LINK_TRANS(link, ib_link, trans);

	/* OO API Only: Verify connection has OO API handle */
	if (ib_link->fbc_connection == NULL) {
		_php_fbird_module_error("Connection has no OO API handle");
		RETURN_FALSE;
	}

	/* OO API Only: Verify transaction has OO API handle */
	if (trans->fbt_transaction == NULL) {
		_php_fbird_module_error("Transaction has no OO API handle");
		RETURN_FALSE;
	}

	snprintf(query, sizeof(query), "SELECT GEN_ID(%s,%ld) FROM rdb$database", generator, inc);

	/* Get attachment from connection */
	attachment = fbc_get_attachment(ib_link->fbc_connection);
	if (!attachment) {
		_php_fbird_module_error("Failed to get attachment from connection");
		RETURN_FALSE;
	}

	/* Get transaction handle */
	transaction_ptr = fbt_get_handle(trans->fbt_transaction);
	if (!transaction_ptr) {
		_php_fbird_module_error("Failed to get transaction handle");
		RETURN_FALSE;
	}

	/* Prepare the query via OO API */
	stmt = fbs_prepare(IBG(master_instance), attachment, transaction_ptr,
		query, (unsigned)strlen(query), SQL_DIALECT_CURRENT, IB_STATUS);
	if (!stmt) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* Execute the statement and fetch the result via OO API */
	result = fbs_execute_singleton_int64(IBG(master_instance), stmt, transaction_ptr, IB_STATUS);

	/* Check for errors (result 0 could be valid, check status) */
	if (IB_STATUS[0] == 1 && IB_STATUS[1] != 0) {
		_php_fbird_error();
		fbs_free(stmt, IB_STATUS);
		RETURN_FALSE;
	}

	/* Free the statement */
	fbs_free(stmt, IB_STATUS);

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

/* =============================================================================
 * Limbo Transaction Functions (Two-Phase Commit Recovery)
 * ============================================================================= */

/* {{{ proto array|false fbird_get_limbo_transactions([resource link_identifier [, int max_count]])
   Get list of limbo (in-doubt) transaction IDs */
PHP_FUNCTION(fbird_get_limbo_transactions)
{
	zval *link_arg = NULL;
	zend_long max_count = 100;
	fbird_db_link *ib_link;
	ISC_INT64 *trans_ids;
	int count, i;
	void *attachment;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|r!l", &link_arg, &max_count) == FAILURE) {
		return;
	}

	if (max_count < 1 || max_count > 10000) {
		php_error_docref(NULL, E_WARNING, "max_count must be between 1 and 10000");
		RETURN_FALSE;
	}

	if (link_arg == NULL) {
		ib_link = (fbird_db_link *)zend_fetch_resource2(IBG(default_link), LE_LINK, le_link, le_plink);
	} else {
		ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
	}

	if (!ib_link) {
		RETURN_FALSE;
	}

	if (ib_link->fbc_connection == NULL) {
		_php_fbird_module_error("Connection has no OO API handle");
		RETURN_FALSE;
	}

	attachment = fbc_get_attachment(ib_link->fbc_connection);
	if (attachment == NULL) {
		_php_fbird_module_error("Failed to get attachment from connection");
		RETURN_FALSE;
	}

	trans_ids = (ISC_INT64 *)safe_emalloc(sizeof(ISC_INT64), (size_t)max_count, 0);

	count = fbt_get_limbo_transactions(IBG(master_instance), attachment, trans_ids,
		(unsigned)max_count, IB_STATUS);

	if (count < 0) {
		efree(trans_ids);
		_php_fbird_error();
		RETURN_FALSE;
	}

	array_init(return_value);
	for (i = 0; i < count; i++) {
		add_next_index_long(return_value, (zend_long)trans_ids[i]);
	}

	efree(trans_ids);
}
/* }}} */

/* {{{ proto resource|false fbird_reconnect_transaction(resource link_identifier, int transaction_id)
   Reconnect to a limbo transaction for recovery */
PHP_FUNCTION(fbird_reconnect_transaction)
{
	zval *link_arg;
	zend_long trans_id;
	fbird_db_link *ib_link;
	fbird_transaction *ib_trans;
	void *attachment;
	void *reconnected_trans;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &link_arg, &trans_id) == FAILURE) {
		return;
	}

	ib_link = (fbird_db_link *)zend_fetch_resource2_ex(link_arg, LE_LINK, le_link, le_plink);
	if (!ib_link) {
		RETURN_FALSE;
	}

	if (ib_link->fbc_connection == NULL) {
		_php_fbird_module_error("Connection has no OO API handle");
		RETURN_FALSE;
	}

	attachment = fbc_get_attachment(ib_link->fbc_connection);
	if (attachment == NULL) {
		_php_fbird_module_error("Failed to get attachment from connection");
		RETURN_FALSE;
	}

	reconnected_trans = fbt_reconnect(IBG(master_instance), attachment, trans_id, IB_STATUS);
	if (reconnected_trans == NULL) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* Allocate and initialize transaction structure */
	ib_trans = (fbird_transaction *)safe_emalloc(1, sizeof(fbird_transaction), 0);
	ib_trans->fbt_transaction = reconnected_trans;
	ib_trans->handle.ptr = fbt_get_handle(reconnected_trans);
	ib_trans->link_cnt = 1;
	ib_trans->affected_rows = 0;
	ib_trans->db_link[0] = ib_link;

	/* Link into connection's transaction list */
	if (ib_link->tr_list == NULL) {
		ib_link->tr_list = (fbird_tr_list *)emalloc(sizeof(fbird_tr_list));
		ib_link->tr_list->trans = NULL;
		ib_link->tr_list->next = NULL;
	}

	fbird_tr_list **l;
	for (l = &ib_link->tr_list; *l != NULL; l = &(*l)->next);
	*l = (fbird_tr_list *)emalloc(sizeof(fbird_tr_list));
	(*l)->trans = ib_trans;
	(*l)->next = NULL;

	RETVAL_RES(zend_register_resource(ib_trans, le_trans));
	Z_TRY_ADDREF_P(return_value);
}
/* }}} */

#if FB_API_VER >= 40
/* =============================================================================
 * IBatch API Functions (Firebird 4.0+ Bulk Operations)
 * ============================================================================= */

/* {{{ proto resource|false fbird_batch_create(resource query [, resource trans_identifier])
   Create a batch from a prepared statement for bulk operations */
PHP_FUNCTION(fbird_batch_create)
{
	zval *query_arg, *trans_arg = NULL;
	fbird_query *ib_query;
	fbird_transaction *trans = NULL;
	fbird_batch *ib_batch;
	void *stmt_ptr;
	void *batch_wrapper;
	void *metadata;
	unsigned msg_length;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r|r!", &query_arg, &trans_arg) == FAILURE) {
		return;
	}

	ib_query = (fbird_query *)zend_fetch_resource_ex(query_arg, LE_QUERY, le_query);
	if (!ib_query) {
		RETURN_FALSE;
	}

	if (!ib_query->fbs_statement) {
		_php_fbird_module_error("Query has no OO API statement handle");
		RETURN_FALSE;
	}

	/* Get transaction - either from parameter or from query's default */
	if (trans_arg != NULL) {
		trans = (fbird_transaction *)zend_fetch_resource_ex(trans_arg, LE_TRANS, le_trans);
		if (!trans) {
			RETURN_FALSE;
		}
	} else {
		trans = ib_query->trans;
	}

	if (!trans || !trans->fbt_transaction) {
		_php_fbird_module_error("No valid transaction for batch operation");
		RETURN_FALSE;
	}

	/* Get raw IStatement pointer */
	stmt_ptr = fbs_get_statement(ib_query->fbs_statement);
	if (!stmt_ptr) {
		_php_fbird_module_error("Failed to get statement handle");
		RETURN_FALSE;
	}

	/* Create batch with default buffer size */
	batch_wrapper = fbbatch_create(IBG(master_instance), stmt_ptr, 0, IB_STATUS);
	if (!batch_wrapper) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* Get input metadata for building message buffers */
	metadata = fbbatch_get_metadata(IBG(master_instance), batch_wrapper, IB_STATUS);
	if (!metadata) {
		fbbatch_close(IBG(master_instance), batch_wrapper, IB_STATUS);
		_php_fbird_error();
		RETURN_FALSE;
	}

	msg_length = fbm_get_message_length(IBG(master_instance), metadata);

	/* Allocate batch structure */
	ib_batch = (fbird_batch *)ecalloc(1, sizeof(fbird_batch));
	ib_batch->fbbatch_wrapper = batch_wrapper;
	ib_batch->trans = trans;
	ib_batch->query = ib_query;
	ib_batch->in_metadata = metadata;
	ib_batch->in_msg_length = msg_length;
	ib_batch->in_msg_buffer = emalloc(msg_length);
	memset(ib_batch->in_msg_buffer, 0, msg_length);

	RETVAL_RES(zend_register_resource(ib_batch, le_batch));
}
/* }}} */

/* {{{ proto bool fbird_batch_add(resource batch, mixed ...$args)
   Add a row of parameters to the batch */
PHP_FUNCTION(fbird_batch_add)
{
	zval *batch_arg;
	zval *args = NULL;
	int argc = 0;
	fbird_batch *ib_batch;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r*", &batch_arg, &args, &argc) == FAILURE) {
		return;
	}

	ib_batch = (fbird_batch *)zend_fetch_resource_ex(batch_arg, LE_BATCH, le_batch);
	if (!ib_batch || !ib_batch->fbbatch_wrapper) {
		php_error_docref(NULL, E_WARNING, "Invalid batch resource");
		RETURN_FALSE;
	}

	/* Get master interface for metadata operations */
	void *master = IBG(master_instance);
	if (!master) {
		_php_fbird_module_error("fbird_batch_add() requires Firebird 3.0+ OO API master interface");
		RETURN_FALSE;
	}

	/* Validate metadata and buffer are available */
	if (!ib_batch->in_metadata || !ib_batch->in_msg_buffer || ib_batch->in_msg_length == 0) {
		_php_fbird_module_error("fbird_batch_add() batch has no input metadata or buffer");
		RETURN_FALSE;
	}

	/* Get parameter count from metadata */
	unsigned param_count = fbm_get_count(master, ib_batch->in_metadata);

	/* Validate argument count matches parameter count */
	if ((unsigned)argc != param_count) {
		_php_fbird_module_error("fbird_batch_add() expects %u parameters, %d given", param_count, argc);
		RETURN_FALSE;
	}

	/* Clear message buffer before populating */
	memset(ib_batch->in_msg_buffer, 0, ib_batch->in_msg_length);

	/* Bind each parameter to the message buffer */
	for (unsigned i = 0; i < param_count; i++) {
		zval *b_var = &args[i];

		/* Get metadata info for this parameter */
		unsigned sql_type = fbm_get_type(master, ib_batch->in_metadata, i) & ~1;
		unsigned data_offset = fbm_get_offset(master, ib_batch->in_metadata, i);
		unsigned null_offset = fbm_get_null_offset(master, ib_batch->in_metadata, i);
		unsigned field_length = fbm_get_length(master, ib_batch->in_metadata, i);
		int sql_scale = fbm_get_scale(master, ib_batch->in_metadata, i);

		/* Get pointers to data and null indicator in message buffer */
		unsigned char *data_ptr = (unsigned char *)ib_batch->in_msg_buffer + data_offset;
		short *null_ptr = (short *)((unsigned char *)ib_batch->in_msg_buffer + null_offset);

		/* Handle NULL values */
		int is_null = 0;
		switch (Z_TYPE_P(b_var)) {
			case IS_NULL:
				is_null = 1;
				break;
			case IS_STRING:
				/* Empty string treated as NULL for numeric/date types */
				if (Z_STRLEN_P(b_var) == 0) {
					switch (sql_type) {
						case SQL_SHORT:
						case SQL_LONG:
						case SQL_INT64:
						case SQL_FLOAT:
						case SQL_DOUBLE:
						case SQL_TIMESTAMP:
						case SQL_TYPE_DATE:
						case SQL_TYPE_TIME:
#if FB_API_VER >= 40
						case SQL_INT128:
						case SQL_DEC16:
						case SQL_DEC34:
						case SQL_TIMESTAMP_TZ:
						case SQL_TIME_TZ:
#endif
							is_null = 1;
							break;
						default:
							break;
					}
				}
				break;
			default:
				break;
		}

		if (is_null) {
			*null_ptr = -1;
			continue;
		}

		/* Not NULL */
		*null_ptr = 0;

		/* Convert PHP value to Firebird format based on SQL type */
		switch (sql_type) {
			case SQL_SHORT: {
				if (sql_scale < 0) {
					/* NUMERIC/DECIMAL with scale */
					double dval = zval_get_double(b_var);
					double factor = pow(10.0, (double)(-sql_scale));
					long long scaled = llround(dval * factor);
					if (scaled < SHRT_MIN || scaled > SHRT_MAX) {
						_php_fbird_module_error("Parameter %u: scaled value out of range for SHORT", i + 1);
						RETURN_FALSE;
					}
					*(short *)data_ptr = (short)scaled;
				} else {
					zend_long lval = zval_get_long(b_var);
					*(short *)data_ptr = (short)lval;
				}
				break;
			}

			case SQL_LONG: {
				if (sql_scale < 0) {
					double dval = zval_get_double(b_var);
					double factor = pow(10.0, (double)(-sql_scale));
					long long scaled = llround(dval * factor);
					if (scaled < INT_MIN || scaled > INT_MAX) {
						_php_fbird_module_error("Parameter %u: scaled value out of range for LONG", i + 1);
						RETURN_FALSE;
					}
					*(ISC_LONG *)data_ptr = (ISC_LONG)scaled;
				} else {
					zend_long lval = zval_get_long(b_var);
					*(ISC_LONG *)data_ptr = (ISC_LONG)lval;
				}
				break;
			}

			case SQL_INT64: {
				if (sql_scale < 0) {
					double dval = zval_get_double(b_var);
					double factor = pow(10.0, (double)(-sql_scale));
					*(ISC_INT64 *)data_ptr = (ISC_INT64)llround(dval * factor);
				} else {
					zend_long lval = zval_get_long(b_var);
					*(ISC_INT64 *)data_ptr = (ISC_INT64)lval;
				}
				break;
			}

			case SQL_FLOAT: {
				double dval = zval_get_double(b_var);
				*(float *)data_ptr = (float)dval;
				break;
			}

			case SQL_DOUBLE: {
				double dval = zval_get_double(b_var);
				*(double *)data_ptr = dval;
				break;
			}

			case SQL_TEXT: {
				/* Fixed-length CHAR field */
				convert_to_string(b_var);
				size_t str_len = Z_STRLEN_P(b_var);
				if (str_len > field_length) {
					str_len = field_length;
				}
				memcpy(data_ptr, Z_STRVAL_P(b_var), str_len);
				/* Pad with spaces for CHAR type */
				if (str_len < field_length) {
					memset(data_ptr + str_len, ' ', field_length - str_len);
				}
				break;
			}

			case SQL_VARYING: {
				/* VARCHAR: 2-byte length prefix + data */
				convert_to_string(b_var);
				size_t str_len = Z_STRLEN_P(b_var);
				if (str_len > field_length) {
					str_len = field_length;
				}
				*(short *)data_ptr = (short)str_len;
				memcpy(data_ptr + sizeof(short), Z_STRVAL_P(b_var), str_len);
				break;
			}

			case SQL_TIMESTAMP:
			case SQL_TYPE_DATE:
			case SQL_TYPE_TIME: {
				if (Z_TYPE_P(b_var) == IS_LONG) {
					/* Unix timestamp */
					struct tm t;
					time_t ts = (time_t)Z_LVAL_P(b_var);
					struct tm *res = php_gmtime_r(&ts, &t);
					if (!res) {
						_php_fbird_module_error("Parameter %u: invalid timestamp value", i + 1);
						RETURN_FALSE;
					}
					switch (sql_type) {
						case SQL_TIMESTAMP:
							*(ISC_TIMESTAMP *)data_ptr = fbu_encode_timestamp(master,
								(unsigned)(t.tm_year + 1900), (unsigned)(t.tm_mon + 1),
								(unsigned)t.tm_mday, (unsigned)t.tm_hour,
								(unsigned)t.tm_min, (unsigned)t.tm_sec, 0);
							break;
						case SQL_TYPE_DATE:
							*(ISC_DATE *)data_ptr = fbu_encode_date(master,
								(unsigned)(t.tm_year + 1900), (unsigned)(t.tm_mon + 1),
								(unsigned)t.tm_mday);
							break;
						case SQL_TYPE_TIME:
							*(ISC_TIME *)data_ptr = fbu_encode_time(master,
								(unsigned)t.tm_hour, (unsigned)t.tm_min,
								(unsigned)t.tm_sec, 0);
							break;
					}
				} else {
					/* Parse string date/time */
					convert_to_string(b_var);
					fbird_datetime_components dt;
					int parsed = 0;
					switch (sql_type) {
						case SQL_TYPE_DATE:
							parsed = fbird_parse_date(Z_STRVAL_P(b_var), &dt);
							if (parsed) {
								*(ISC_DATE *)data_ptr = fbu_encode_date(master, dt.year, dt.month, dt.day);
							}
							break;
						case SQL_TYPE_TIME:
							parsed = fbird_parse_time(Z_STRVAL_P(b_var), &dt);
							if (parsed) {
								*(ISC_TIME *)data_ptr = fbu_encode_time(master, dt.hours, dt.minutes, dt.seconds, dt.fractions);
							}
							break;
						default: /* SQL_TIMESTAMP */
							parsed = fbird_parse_timestamp(Z_STRVAL_P(b_var), &dt);
							if (parsed) {
								*(ISC_TIMESTAMP *)data_ptr = fbu_encode_timestamp(master,
									dt.year, dt.month, dt.day, dt.hours, dt.minutes, dt.seconds, dt.fractions);
							}
							break;
					}
					if (!parsed) {
						_php_fbird_module_error("Parameter %u: invalid date/time string '%s'", i + 1, Z_STRVAL_P(b_var));
						RETURN_FALSE;
					}
				}
				break;
			}

#if FB_API_VER >= 40
			case SQL_TIMESTAMP_TZ:
			case SQL_TIME_TZ: {
				fbird_datetime_components dt;
				fbird_datetime_init(&dt);
				strncpy(dt.timezone, "GMT", sizeof(dt.timezone) - 1);

				if (Z_TYPE_P(b_var) == IS_LONG) {
					struct tm t;
					time_t ts = (time_t)Z_LVAL_P(b_var);
					struct tm *res = php_gmtime_r(&ts, &t);
					if (!res) {
						_php_fbird_module_error("Parameter %u: invalid timestamp value", i + 1);
						RETURN_FALSE;
					}
					dt.year = (unsigned)(t.tm_year + 1900);
					dt.month = (unsigned)(t.tm_mon + 1);
					dt.day = (unsigned)t.tm_mday;
					dt.hours = (unsigned)t.tm_hour;
					dt.minutes = (unsigned)t.tm_min;
					dt.seconds = (unsigned)t.tm_sec;
				} else {
					convert_to_string(b_var);
					int parsed = (sql_type == SQL_TIME_TZ)
						? fbird_parse_time(Z_STRVAL_P(b_var), &dt)
						: fbird_parse_timestamp(Z_STRVAL_P(b_var), &dt);
					if (!parsed) {
						_php_fbird_module_error("Parameter %u: invalid date/time string", i + 1);
						RETURN_FALSE;
					}
					if (!dt.has_timezone) {
						strncpy(dt.timezone, "GMT", sizeof(dt.timezone) - 1);
					}
				}

				if (sql_type == SQL_TIME_TZ) {
					if (fbu_encode_time_tz(master, (ISC_TIME_TZ *)data_ptr,
							dt.hours, dt.minutes, dt.seconds, dt.fractions, dt.timezone) != 0) {
						_php_fbird_module_error("Parameter %u: failed to encode TIME WITH TIME ZONE", i + 1);
						RETURN_FALSE;
					}
				} else {
					if (fbu_encode_timestamp_tz(master, (ISC_TIMESTAMP_TZ *)data_ptr,
							dt.year, dt.month, dt.day, dt.hours, dt.minutes, dt.seconds,
							dt.fractions, dt.timezone) != 0) {
						_php_fbird_module_error("Parameter %u: failed to encode TIMESTAMP WITH TIME ZONE", i + 1);
						RETURN_FALSE;
					}
				}
				break;
			}
#endif

#ifdef SQL_BOOLEAN
			case SQL_BOOLEAN: {
				FB_BOOLEAN bval;
				switch (Z_TYPE_P(b_var)) {
					case IS_TRUE:
						bval = FB_TRUE;
						break;
					case IS_FALSE:
						bval = FB_FALSE;
						break;
					case IS_LONG:
					case IS_DOUBLE:
						bval = zend_is_true(b_var) ? FB_TRUE : FB_FALSE;
						break;
					case IS_STRING:
						if (Z_STRLEN_P(b_var) == 0) {
							bval = FB_FALSE;
						} else if (!zend_binary_strncasecmp(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), "true", 4, 4)) {
							bval = FB_TRUE;
						} else if (!zend_binary_strncasecmp(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), "false", 5, 5)) {
							bval = FB_FALSE;
						} else {
							zend_long lval;
							double dval;
							switch (is_numeric_string(Z_STRVAL_P(b_var), Z_STRLEN_P(b_var), &lval, &dval, 0)) {
								case IS_LONG:
									bval = (lval != 0) ? FB_TRUE : FB_FALSE;
									break;
								case IS_DOUBLE:
									bval = (dval != 0) ? FB_TRUE : FB_FALSE;
									break;
								default:
									_php_fbird_module_error("Parameter %u: cannot convert string to boolean", i + 1);
									RETURN_FALSE;
							}
						}
						break;
					default:
						bval = zend_is_true(b_var) ? FB_TRUE : FB_FALSE;
						break;
				}
				*(FB_BOOLEAN *)data_ptr = bval;
				break;
			}
#endif

			case SQL_BLOB: {
				/* BLOB ID as hex string "HHHHHHHH:LLLL" (13 characters)
				 * Format: 8 hex digits (high 32-bit), colon, 4 hex digits (low 16-bit)
				 * Example: "74292B00:7FFC"
				 */
				convert_to_string(b_var);

				if (Z_STRLEN_P(b_var) == BLOB_ID_LEN &&
					_php_fbird_string_to_quad(Z_STRVAL_P(b_var), (ISC_QUAD *)data_ptr)) {
					/* Valid BLOB ID parsed and written to message buffer */
					break;
				}

				/* Invalid BLOB ID format */
				_php_fbird_module_error("Parameter %u: BLOB must be passed as blob ID (use fbird_batch_add_blob() or fbird_blob_create())", i + 1);
				RETURN_FALSE;
			}

			default:
				_php_fbird_module_error("Parameter %u: unsupported SQL type %u for batch binding", i + 1, sql_type);
				RETURN_FALSE;
		}
	}

	/* Add the populated message buffer to the batch */
	if (fbbatch_add(master, ib_batch->fbbatch_wrapper, 1, ib_batch->in_msg_buffer, IB_STATUS) != 1) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array|false fbird_batch_execute(resource batch)
   Execute the batch and return results */
PHP_FUNCTION(fbird_batch_execute)
{
	zval *batch_arg;
	fbird_batch *ib_batch;
	void *trans_ptr;
	unsigned total_processed = 0;
	unsigned error_count = 0;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &batch_arg) == FAILURE) {
		return;
	}

	ib_batch = (fbird_batch *)zend_fetch_resource_ex(batch_arg, LE_BATCH, le_batch);
	if (!ib_batch || !ib_batch->fbbatch_wrapper) {
		php_error_docref(NULL, E_WARNING, "Invalid batch resource");
		RETURN_FALSE;
	}

	if (!ib_batch->trans || !ib_batch->trans->fbt_transaction) {
		_php_fbird_module_error("Batch has no valid transaction");
		RETURN_FALSE;
	}

	trans_ptr = fbt_get_handle(ib_batch->trans->fbt_transaction);
	if (!trans_ptr) {
		_php_fbird_module_error("Failed to get transaction handle");
		RETURN_FALSE;
	}

	if (!fbbatch_execute(IBG(master_instance), ib_batch->fbbatch_wrapper, trans_ptr,
			&total_processed, &error_count, IB_STATUS)) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* Close the batch after execution */
	fbbatch_close(IBG(master_instance), ib_batch->fbbatch_wrapper, IB_STATUS);
	ib_batch->fbbatch_wrapper = NULL;

	/* Calculate success_count from total_processed - error_count */
	unsigned success_count = (total_processed >= error_count) ? (total_processed - error_count) : 0;

	array_init(return_value);
	add_assoc_long(return_value, "total_processed", total_processed);
	add_assoc_long(return_value, "success_count", success_count);
	add_assoc_long(return_value, "error_count", error_count);
}
/* }}} */

/* {{{ proto bool fbird_batch_cancel(resource batch)
   Cancel the batch without executing */
PHP_FUNCTION(fbird_batch_cancel)
{
	zval *batch_arg;
	fbird_batch *ib_batch;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &batch_arg) == FAILURE) {
		return;
	}

	ib_batch = (fbird_batch *)zend_fetch_resource_ex(batch_arg, LE_BATCH, le_batch);
	if (!ib_batch || !ib_batch->fbbatch_wrapper) {
		php_error_docref(NULL, E_WARNING, "Invalid batch resource");
		RETURN_FALSE;
	}

	if (!fbbatch_cancel(IBG(master_instance), ib_batch->fbbatch_wrapper, IB_STATUS)) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	fbbatch_close(IBG(master_instance), ib_batch->fbbatch_wrapper, IB_STATUS);
	ib_batch->fbbatch_wrapper = NULL;

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string|false fbird_batch_add_blob(resource batch, string data [, int type])
   Create inline BLOB in batch context and return BLOB ID */
PHP_FUNCTION(fbird_batch_add_blob)
{
	zval *batch_arg;
	char *data;
	size_t data_len;
	zend_long blob_type = 0;
	fbird_batch *ib_batch;
	ISC_QUAD blob_id;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs|l", &batch_arg, &data, &data_len, &blob_type) == FAILURE) {
		return;
	}

	ib_batch = (fbird_batch *)zend_fetch_resource_ex(batch_arg, LE_BATCH, le_batch);
	if (!ib_batch || !ib_batch->fbbatch_wrapper) {
		php_error_docref(NULL, E_WARNING, "Invalid batch resource");
		RETURN_FALSE;
	}

	/* Call C++ wrapper to add BLOB to batch */
	if (fbbatch_add_blob(IBG(master_instance), ib_batch->fbbatch_wrapper,
			(unsigned)data_len, data, &blob_id, 0, NULL, IB_STATUS) == 0) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* Convert BLOB ID to hex string for PHP */
	RETURN_NEW_STR(_php_fbird_quad_to_string(blob_id));
}
/* }}} */

/* {{{ proto string|false fbird_batch_register_blob(resource batch, string blob_id)
   Register existing BLOB for batch use */
PHP_FUNCTION(fbird_batch_register_blob)
{
	zval *batch_arg;
	char *blob_id_str;
	size_t blob_id_len;
	fbird_batch *ib_batch;
	ISC_QUAD existing_blob, batch_blob_id;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &batch_arg, &blob_id_str, &blob_id_len) == FAILURE) {
		return;
	}

	ib_batch = (fbird_batch *)zend_fetch_resource_ex(batch_arg, LE_BATCH, le_batch);
	if (!ib_batch || !ib_batch->fbbatch_wrapper) {
		php_error_docref(NULL, E_WARNING, "Invalid batch resource");
		RETURN_FALSE;
	}

	/* Validate and convert BLOB ID string to ISC_QUAD */
	if (blob_id_len != BLOB_ID_LEN || !_php_fbird_string_to_quad(blob_id_str, &existing_blob)) {
		php_error_docref(NULL, E_WARNING, "Invalid BLOB ID format (expected %d character hex string)", BLOB_ID_LEN);
		RETURN_FALSE;
	}

	/* Call C++ wrapper to register BLOB in batch */
	if (fbbatch_register_blob(IBG(master_instance), ib_batch->fbbatch_wrapper,
			&existing_blob, &batch_blob_id, IB_STATUS) == 0) {
		_php_fbird_error();
		RETURN_FALSE;
	}

	/* Convert batch BLOB ID to hex string for PHP */
	RETURN_NEW_STR(_php_fbird_quad_to_string(batch_blob_id));
}
/* }}} */
#endif /* FB_API_VER >= 40 */


#endif /* HAVE_FIREBIRD */

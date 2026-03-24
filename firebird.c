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
#ifndef PHP_WIN32
#include <dlfcn.h>
#endif
#include "firebird_utils.h"
#include "fbird_datetime.h"
#include "fbird_classes.h"
#include "php_fbird_connection.h"
#include "php_fbird_transaction.h"
#include "php_fbird_batch.h"

#ifdef HAVE_PDO_FBIRD
#include "pdo_fbird/php_pdo_fbird.h"
#endif

#define ROLLBACK    0
#define COMMIT      1
#define RETAIN      2

/* CHECK_LINK is now defined in php_fbird_includes.h */

ZEND_DECLARE_MODULE_GLOBALS(fbird)
static PHP_GINIT_FUNCTION(fbird);

zend_class_entry *firebird_exception_ce;

ZEND_BEGIN_ARG_INFO(arginfo_fbird_errmsg, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fbird_errcode, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fbird_sqlstate, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_escape_string, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_set_exception_mode, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, mode, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_fbird_get_exception_mode, 0)
ZEND_END_ARG_INFO()

/* Firebird\Exception method arginfo */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_firebird_exception_getSqlState, 0, 0, IS_STRING, 0)
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
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_drop_db, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_create_database, 0, 0, 1)
	ZEND_ARG_INFO(0, database)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, password)
	ZEND_ARG_INFO(0, charset)
	ZEND_ARG_INFO(0, page_size)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_prepare_ex, 0, 0, 2)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, query)
	ZEND_ARG_INFO(0, trans_handle)
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_last_insert_id, 0, 0, 1)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, sequence)
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_query_params_tx, 0, 0, 3)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_INFO(0, trans_handle)
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
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

/* Limbo Transaction Functions (Two-Phase Commit Recovery) */
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_get_limbo_transactions, 0, 0, 0)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, max_count, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_reconnect_transaction, 0, 0, 2)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, transaction_id, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* IBatch API Functions (Firebird 4.0+ Bulk Operations) */
#if FB_API_VER >= 40
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_create, 0, 0, 1)
	ZEND_ARG_INFO(0, query)
	ZEND_ARG_INFO(0, trans_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_add, 0, 0, 1)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_VARIADIC_INFO(0, bind_args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_execute, 0, 0, 1)
	ZEND_ARG_INFO(0, batch)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_cancel, 0, 0, 1)
	ZEND_ARG_INFO(0, batch)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_add_blob, 0, 0, 2)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, blob_type, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_register_blob, 0, 0, 2)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, blob_id, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_get_blob_alignment, 0, 0, 1)
	ZEND_ARG_INFO(0, batch)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_append_blob_data, 0, 0, 2)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_add_blob_stream, 0, 0, 2)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_batch_set_default_bpb, 0, 0, 2)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, bpb, IS_STRING, 0)
ZEND_END_ARG_INFO()
#endif /* FB_API_VER >= 40 */

static const zend_function_entry fbird_functions[] = {
	PHP_FE(fbird_connect, 		arginfo_fbird_connect)
	PHP_FE(fbird_pconnect, 		arginfo_fbird_pconnect)
	PHP_FE(fbird_close, 		arginfo_fbird_close)
	PHP_FE(fbird_drop_db, 		arginfo_fbird_drop_db)
	PHP_FE(fbird_create_database, arginfo_fbird_create_database)
	PHP_FE(fbird_prepare_ex, 	arginfo_fbird_prepare_ex)
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
    PHP_FE(fbird_query_params_tx, arginfo_fbird_query_params_tx)

	PHP_FE(fbird_gen_id, 		arginfo_fbird_gen_id)
	PHP_FE(fbird_last_insert_id, arginfo_fbird_last_insert_id)
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
	PHP_FE(fbird_escape_string, arginfo_fbird_escape_string)
	PHP_FE(fbird_set_exception_mode, arginfo_fbird_set_exception_mode)
	PHP_FE(fbird_get_exception_mode, arginfo_fbird_get_exception_mode)

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
	PHP_FE(fbird_batch_get_blob_alignment, arginfo_fbird_batch_get_blob_alignment)
	PHP_FE(fbird_batch_append_blob_data, arginfo_fbird_batch_append_blob_data)
	PHP_FE(fbird_batch_add_blob_stream, arginfo_fbird_batch_add_blob_stream)
	PHP_FE(fbird_batch_set_default_bpb, arginfo_fbird_batch_set_default_bpb)
#endif /* FB_API_VER >= 40 */

	PHP_FE_END
};

zend_module_entry firebird_module_entry = {
	STANDARD_MODULE_HEADER,
	"firebird",
	fbird_functions,
	PHP_MINIT(fbird),
	PHP_MSHUTDOWN(fbird),
	PHP_RINIT(fbird),
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

/* Fill ib_link and trans with the correct database link and transaction. */
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

static PHP_INI_MH(OnUpdateExceptionMode)
{
	if (new_value && zend_ini_parse_bool(new_value)) {
		IBG(exception_mode) = FBIRD_EXCEPTION_MODE_THROW;
	} else {
		IBG(exception_mode) = FBIRD_EXCEPTION_MODE_SILENT;
	}
	return SUCCESS;
}

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
	PHP_INI_ENTRY_EX("fbird.enable_exceptions", "0", PHP_INI_ALL, OnUpdateExceptionMode, zend_ini_boolean_displayer_cb)
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

	/* Store initial PID for fork-safety detection (Issue #22)
	 * After pcntl_fork(), child processes inherit global state including
	 * master_instance pointer. Destructors must skip cleanup in forked
	 * children to avoid segfault from invalid handle cleanup. */
#ifndef PHP_WIN32
	fbird_globals->init_pid = getpid();
#else
	fbird_globals->init_pid = 0;
#endif

	/* Exception mode: SILENT (0) by default for backward compatibility.
	 * Users can opt-in to THROW via fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW)
	 * or fbird.enable_exceptions=1 INI setting. */
	fbird_globals->exception_mode = FBIRD_EXCEPTION_MODE_SILENT;

	/* MSHUTDOWN detection flag for safe persistent resource cleanup (Issue #50, #51) */
	fbird_globals->in_mshutdown = 0;
}

PHP_MINIT_FUNCTION(fbird)
{
	REGISTER_INI_ENTRIES();

	zend_class_entry ce;
	zend_class_entry *runtime_ce = zend_hash_str_find_ptr(CG(class_table),
		"runtimeexception", sizeof("runtimeexception") - 1);
	INIT_CLASS_ENTRY(ce, "Firebird\\Exception", firebird_exception_methods);
	firebird_exception_ce = zend_register_internal_class_ex(&ce,
		runtime_ce ? runtime_ce : zend_ce_exception);

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

	/* Exception mode constants */
	REGISTER_LONG_CONSTANT("FBIRD_EXCEPTION_MODE_SILENT", FBIRD_EXCEPTION_MODE_SILENT, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_EXCEPTION_MODE_THROW", FBIRD_EXCEPTION_MODE_THROW, CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("FBIRD_EXCEPTION_MODE_COMPAT", FBIRD_EXCEPTION_MODE_SILENT, CONST_PERSISTENT);

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

	fbird_register_classes();

	atexit(_fbird_process_exit_handler);

	php_fbird_query_minit(INIT_FUNC_ARGS_PASSTHRU);
	php_fbird_blobs_minit(INIT_FUNC_ARGS_PASSTHRU);
	php_fbird_events_minit(INIT_FUNC_ARGS_PASSTHRU);
	php_fbird_service_minit(INIT_FUNC_ARGS_PASSTHRU);

#ifdef HAVE_PDO_FBIRD
	/* Initialize integrated pdo_fbird PDO driver */
	if (PHP_MINIT(pdo_fbird)(INIT_FUNC_ARGS_PASSTHRU) == FAILURE) {
		return FAILURE;
	}
#endif

#ifdef ZEND_SIGNALS
	// firebird replaces some signals at runtime, suppress warnings.
	SIGG(check) = 0;
#endif

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(fbird)
{
	/* Set in_mshutdown flag FIRST to prevent EG() access in persistent resource destructors.
	 * During MSHUTDOWN, EG(regular_list) and EG(persistent_list) may already be destroyed.
	 * Fixes: Issue #50 (SIGSEGV exit code 139), Issue #51 (EG() access during MSHUTDOWN)
	 */
	IBG(in_mshutdown) = 1;

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
#ifdef HAVE_PDO_FBIRD
	PHP_MSHUTDOWN(pdo_fbird)(SHUTDOWN_FUNC_ARGS_PASSTHRU);
#endif

	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(fbird)
{
	IBG(in_mshutdown) = 0;
	fbird_set_shutdown_active(0);
	return SUCCESS;
}
/* }}} */

PHP_RSHUTDOWN_FUNCTION(fbird)
{
	/* Set shutdown flag immediately at start of request shutdown.
	 * Any Firebird objects destroyed after this will skip detach() calls. */
	fbird_set_shutdown_active(1);
	IBG(in_mshutdown) = 1;
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
	typedef void (*info_func_t)(char *);
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

#endif /* HAVE_FIREBIRD */

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
#include "fbird_classes.h"
#include "php_fbird_connection.h"
#include "php_fbird_transaction.h"
#include "php_fbird_batch.h"

#define ROLLBACK    0
#define COMMIT      1
#define RETAIN      2

/* CHECK_LINK is now defined in php_fbird_includes.h */

ZEND_DECLARE_MODULE_GLOBALS(fbird)
static PHP_GINIT_FUNCTION(fbird);

zend_class_entry *firebird_exception_ce;

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_errmsg, 0, 0, MAY_BE_STRING|MAY_BE_FALSE)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_errcode, 0, 0, MAY_BE_LONG|MAY_BE_FALSE)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_sqlstate, 0, 0, MAY_BE_STRING|MAY_BE_FALSE)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_escape_string, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, string, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_set_exception_mode, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, mode, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_get_exception_mode, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_connect, 0, 0, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, database, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, username, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, charset, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, buffers, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, dialect, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, role, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_pconnect, 0, 0, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, database, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, username, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, charset, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, buffers, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, dialect, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, role, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_close, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_drop_db, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_create_database, 0, 1, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, database, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, username, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, charset, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, page_size, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_prepare_ex, 0, 2, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
	ZEND_ARG_INFO(0, trans_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_trans, 0, 0, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_VARIADIC_INFO(0, trans_args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_trans_start, 0, 1, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, options, IS_ARRAY, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_savepoint, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, trans_handle)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_trans_info, 0, 1, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, trans_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_commit, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_rollback, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_commit_ret, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_rollback_ret, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_gen_id, 0, 1, MAY_BE_LONG|MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, generator, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, increment, IS_LONG, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_last_insert_id, 0, 1, MAY_BE_LONG|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, sequence, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_create, 0, 0, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_open, 0, 0, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, blob_id, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_blob_add, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, blob_handle)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_get, 0, 2, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, blob_handle)
	ZEND_ARG_TYPE_INFO(0, len, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_close, 0, 1, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, blob_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_blob_cancel, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, blob_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_info, 0, 0, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, blob_id, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_blob_echo, 0, 0, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, blob_id, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_import, 0, 0, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_create_seekable, 0, 0, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_open_seekable, 0, 1, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, blob_id, IS_STRING, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_blob_seek, 0, 2, MAY_BE_LONG|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, blob_handle)
	ZEND_ARG_TYPE_INFO(0, offset, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, whence, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_query, 0, 0, MAY_BE_RESOURCE|MAY_BE_BOOL)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
	ZEND_ARG_VARIADIC_INFO(0, bind_arg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_affected_rows, 0, 0, IS_LONG, 0)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_fetch_row, 0, 1, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_TYPE_INFO(0, fetch_flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_fetch_assoc, 0, 1, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_TYPE_INFO(0, fetch_flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_fetch_object, 0, 1, MAY_BE_OBJECT|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_TYPE_INFO(0, fetch_flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_name_result, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, result)
	ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_free_result, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, result)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_prepare, 0, 0, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_execute, 0, 1, MAY_BE_RESOURCE|MAY_BE_BOOL)
	ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
	ZEND_ARG_VARIADIC_INFO(0, bind_arg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_free_query, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_list_table_blockers, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_TYPE_INFO(0, table_name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_kill_attachment, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_TYPE_INFO(0, attachment_id, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_drop_table_force, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_TYPE_INFO(0, table_name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_execute_statement, 0, 2, MAY_BE_LONG|MAY_BE_FALSE)
    ZEND_ARG_INFO(0, trans_handle)
    ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_execute_query, 0, 2, MAY_BE_RESOURCE|MAY_BE_FALSE)
    ZEND_ARG_INFO(0, trans_handle)
    ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_execute_auto, 0, 2, MAY_BE_RESOURCE|MAY_BE_LONG|MAY_BE_FALSE)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_query_params_tx, 0, 3, MAY_BE_RESOURCE|MAY_BE_LONG|MAY_BE_FALSE)
    ZEND_ARG_INFO(0, link_identifier)
    ZEND_ARG_INFO(0, trans_handle)
    ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_num_fields, 0, 1, MAY_BE_LONG|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, query_result)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_field_info, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, query_result)
	ZEND_ARG_TYPE_INFO(0, field_number, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_num_params, 0, 1, MAY_BE_LONG|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_param_info, 0, 2, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, field_number, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_add_user, 0, 3, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_TYPE_INFO(0, user_name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, first_name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, middle_name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, last_name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_modify_user, 0, 3, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_TYPE_INFO(0, user_name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, password, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, first_name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, middle_name, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, last_name, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* fbird_delete_user($svc, $user_name): service-based user deletion does not
 * require a password (the service handle carries the privilege). The C
 * implementation parses "zs" (exactly 2 args). See specs/spec-v11.0.1-hotfixes.md HF-3. */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_delete_user, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_TYPE_INFO(0, user_name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_service_attach, 0, 0, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, host, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, dba_username, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, dba_password, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_service_detach, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, service_handle)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_backup, 0, 3, MAY_BE_STRING|MAY_BE_BOOL)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_TYPE_INFO(0, source_db, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, dest_file, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, options, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, verbose, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_restore, 0, 3, MAY_BE_STRING|MAY_BE_BOOL)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_TYPE_INFO(0, source_file, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, dest_db, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, options, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, verbose, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_maintain_db, 0, 3, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_TYPE_INFO(0, db, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, action, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, argument, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_db_info, 0, 3, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_TYPE_INFO(0, db, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, action, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, argument, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_server_info, 0, 2, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, service_handle)
	ZEND_ARG_TYPE_INFO(0, action, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_wait_event, 0, 1, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_INFO(0, event)
	ZEND_ARG_INFO(0, event2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_set_event_handler, 0, 2, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, handler, IS_CALLABLE, 0)
	ZEND_ARG_INFO(0, event)
	ZEND_ARG_INFO(0, event2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_poll_event, 0, 1, MAY_BE_ARRAY|MAY_BE_LONG|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, event)
	ZEND_ARG_TYPE_INFO(0, timeout_ms, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_free_event_handler, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, event)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_get_client_version, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_get_client_major_version, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_get_client_minor_version, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_connection_info, 0, 0, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
ZEND_END_ARG_INFO()

/* Limbo Transaction Functions (Two-Phase Commit Recovery) */
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_get_limbo_transactions, 0, 0, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, max_count, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_reconnect_transaction, 0, 2, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, link_identifier)
	ZEND_ARG_TYPE_INFO(0, transaction_id, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* IBatch API Functions (Firebird 4.0+ Bulk Operations) */
#if FB_API_VER >= 40
ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_batch_create, 0, 1, MAY_BE_RESOURCE|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, query, IS_STRING, 0)
	ZEND_ARG_INFO(0, trans_identifier)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_batch_add, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_VARIADIC_INFO(0, bind_args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_batch_execute, 0, 1, MAY_BE_ARRAY|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, batch)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_batch_cancel, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, batch)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_batch_add_blob, 0, 2, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, blob_type, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_batch_register_blob, 0, 2, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, blob_id, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_fbird_batch_get_blob_alignment, 0, 1, MAY_BE_LONG|MAY_BE_FALSE)
	ZEND_ARG_INFO(0, batch)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_batch_append_blob_data, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_batch_add_blob_stream, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_INFO(0, batch)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_fbird_batch_set_default_bpb, 0, 2, _IS_BOOL, 0)
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

/* Helper function to get human-readable name for resource types.
 * Used by FBIRD_VALIDATE_*_EX macros for TypeError messages.
 * Note: Only handles globally-visible resource types; local types
 * (le_blob, le_event) will return "resource". */
const char *_fbird_res_type_name(int type) {
	if (type == le_link)  return "connection";
	if (type == le_plink) return "persistent connection";
	if (type == le_trans) return "transaction";
	if (type == le_query) return "query/result";
#if FB_API_VER >= 40
	if (type == le_batch) return "batch";
#endif
	/* For static resource types defined in other files, return generic name */
	return "resource";
}


/* Fill fb_link and trans with the correct database link and transaction. */
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
		FBG(exception_mode) = FBIRD_EXCEPTION_MODE_THROW;
	} else {
		FBG(exception_mode) = FBIRD_EXCEPTION_MODE_SILENT;
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
	PHP_INI_ENTRY("fbird.timestampformat", FB_DEF_DATE_FMT " " FB_DEF_TIME_FMT, PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("fbird.dateformat", FB_DEF_DATE_FMT, PHP_INI_ALL, NULL)
	PHP_INI_ENTRY("fbird.timeformat", FB_DEF_TIME_FMT, PHP_INI_ALL, NULL)
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
	/* Set in_mshutdown flag FIRST to prevent EG() access in persistent resource destructors.
	 * During MSHUTDOWN, EG(regular_list) and EG(persistent_list) may already be destroyed.
	 * Fixes: Issue #50 (SIGSEGV exit code 139), Issue #51 (EG() access during MSHUTDOWN)
	 */
	FBG(in_mshutdown) = 1;

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

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(fbird)
{
	return SUCCESS;
}
/* }}} */

PHP_RSHUTDOWN_FUNCTION(fbird)
{
	FBG(num_links) = FBG(num_persistent);

	/* Properly release the default_link reference (Issue #183).
	 * Previously this just set the pointer to NULL, orphaning the resource
	 * reference (refcount never decremented). This caused the resource to
	 * leak or trigger SIGSEGV during shutdown when the orphaned resource
	 * was cleaned up with stale state. */
	if (FBG(default_link)) {
		zend_list_delete(FBG(default_link));
		FBG(default_link) = NULL;
	}

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

PHP_FUNCTION(fbird_gen_id)
{
	zval *link = NULL;
	char query[128], *generator;
	size_t gen_len;
	zend_long inc = 1;
	fbird_db_link *fb_link = NULL;
	fbird_transaction *trans = NULL;
	ISC_INT64 result = 0;
	void *attachment = NULL;
	void *transaction_ptr = NULL;
	void *stmt = NULL;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "s|lz!", &generator, &gen_len,
			&inc, &link)) {
		RETURN_FALSE;
	}

	if (link) {
		FBIRD_VALIDATE_LINK_EX(link, 3, fb_link);
	}

	if (gen_len > 31) {
		php_error_docref(NULL, E_WARNING, "Invalid generator name (length > 31 characters)");
		RETURN_FALSE;
	}

	if (!is_valid_identifier(generator, gen_len)) {
		php_error_docref(NULL, E_WARNING, "Invalid generator name (contains invalid characters)");
		RETURN_FALSE;
	}

	PHP_FBIRD_LINK_TRANS(link, fb_link, trans);

	/* OO API Only: Verify connection has OO API handle */
	if (fb_link->fbc_connection == NULL) {
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
	attachment = fbc_get_attachment(fb_link->fbc_connection);
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
	stmt = fbs_prepare(FBG(master_instance), attachment, transaction_ptr,
		query, (unsigned)strlen(query), SQL_DIALECT_CURRENT, IB_STATUS);
	if (!stmt) {
		_php_fbird_error(IB_STATUS);
		RETURN_FALSE;
	}

	/* Execute the statement and fetch the result via OO API */
	result = fbs_execute_singleton_int64(FBG(master_instance), stmt, transaction_ptr, IB_STATUS);

	/* Check for errors (result 0 could be valid, check status) */
	if (IB_STATUS[0] == 1 && IB_STATUS[1] != 0) {
		_php_fbird_error(IB_STATUS);
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

/* {{{ proto mixed fbird_last_insert_id(resource link_identifier [, string sequence])
   Returns the last generated value for a sequence/generator without incrementing.
   If sequence is omitted, returns false (Firebird has no implicit last-insert-id). */
PHP_FUNCTION(fbird_last_insert_id)
{
	zval *link = NULL;
	char *sequence = NULL;
	size_t seq_len = 0;
	fbird_db_link *fb_link = NULL;
	fbird_transaction *trans = NULL;

	RESET_ERRMSG;

	if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "z!|s", &link, &sequence, &seq_len)) {
		RETURN_FALSE;
	}

	if (!sequence || seq_len == 0) {
		/* Firebird has no implicit last-insert-id — sequence name is required */
		php_error_docref(NULL, E_WARNING,
			"Firebird requires a sequence/generator name to retrieve the last generated value");
		RETURN_FALSE;
	}

	if (seq_len > 31) {
		php_error_docref(NULL, E_WARNING, "Invalid sequence name (length > 31 characters)");
		RETURN_FALSE;
	}

	if (!is_valid_identifier(sequence, seq_len)) {
		php_error_docref(NULL, E_WARNING, "Invalid sequence name (contains invalid characters)");
		RETURN_FALSE;
	}

	if (link) {
		FBIRD_VALIDATE_LINK_EX(link, 1, fb_link);
	}

	PHP_FBIRD_LINK_TRANS(link, fb_link, trans);

	if (fb_link->fbc_connection == NULL) {
		_php_fbird_module_error("Connection has no OO API handle");
		RETURN_FALSE;
	}

	if (trans->fbt_transaction == NULL) {
		_php_fbird_module_error("Transaction has no OO API handle");
		RETURN_FALSE;
	}

	char query[128];
	snprintf(query, sizeof(query), "SELECT GEN_ID(%s,0) FROM rdb$database", sequence);

	void *attachment = fbc_get_attachment(fb_link->fbc_connection);
	if (!attachment) {
		_php_fbird_module_error("Failed to get attachment from connection");
		RETURN_FALSE;
	}

	void *transaction_ptr = fbt_get_handle(trans->fbt_transaction);
	if (!transaction_ptr) {
		_php_fbird_module_error("Failed to get transaction handle");
		RETURN_FALSE;
	}

	void *stmt = fbs_prepare(FBG(master_instance), attachment, transaction_ptr,
		query, (unsigned)strlen(query), SQL_DIALECT_CURRENT, IB_STATUS);
	if (!stmt) {
		_php_fbird_error(IB_STATUS);
		RETURN_FALSE;
	}

	ISC_INT64 result = fbs_execute_singleton_int64(FBG(master_instance), stmt, transaction_ptr, IB_STATUS);

	if (IB_STATUS[0] == 1 && IB_STATUS[1] != 0) {
		_php_fbird_error(IB_STATUS);
		fbs_free(stmt, IB_STATUS);
		RETURN_FALSE;
	}

	fbs_free(stmt, IB_STATUS);

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
/* }}} */

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

	// FBG(sql_code) = -999; /* no SQL error */

	php_error(level, "%s", buf);
}

/* Limbo Transaction Functions (Two-Phase Commit Recovery) */
PHP_FUNCTION(fbird_get_limbo_transactions)
{
	zval *link_arg = NULL;
	zend_long max_count = 100;
	fbird_db_link *fb_link;
	ISC_INT64 *trans_ids;
	int count, i;
	void *attachment;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|z!l", &link_arg, &max_count) == FAILURE) {
		return;
	}

	if (max_count < 1 || max_count > 10000) {
		php_error_docref(NULL, E_WARNING, "max_count must be between 1 and 10000");
		RETURN_FALSE;
	}

	if (link_arg == NULL) {
		fb_link = (fbird_db_link *)zend_fetch_resource2(FBG(default_link), LE_LINK, le_link, le_plink);
	} else {
		FBIRD_VALIDATE_LINK_EX(link_arg, 1, fb_link);
	}

	if (!fb_link) {
		RETURN_FALSE;
	}

	if (fb_link->fbc_connection == NULL) {
		_php_fbird_module_error("Connection has no OO API handle");
		RETURN_FALSE;
	}

	attachment = fbc_get_attachment(fb_link->fbc_connection);
	if (attachment == NULL) {
		_php_fbird_module_error("Failed to get attachment from connection");
		RETURN_FALSE;
	}

	trans_ids = (ISC_INT64 *)safe_emalloc(sizeof(ISC_INT64), (size_t)max_count, 0);

	count = fbt_get_limbo_transactions(FBG(master_instance), attachment, trans_ids,
		(unsigned)max_count, IB_STATUS);

	if (count < 0) {
		efree(trans_ids);
		_php_fbird_error(IB_STATUS);
		RETURN_FALSE;
	}

	array_init(return_value);
	for (i = 0; i < count; i++) {
		add_next_index_long(return_value, (zend_long)trans_ids[i]);
	}

	efree(trans_ids);
}

PHP_FUNCTION(fbird_reconnect_transaction)
{
	zval *link_arg;
	zend_long trans_id;
	fbird_db_link *fb_link;
	fbird_transaction *fb_trans;
	void *attachment;
	void *reconnected_trans;

	RESET_ERRMSG;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "zl", &link_arg, &trans_id) == FAILURE) {
		return;
	}

	FBIRD_VALIDATE_LINK_EX(link_arg, 1, fb_link);
	if (!fb_link) {
		RETURN_FALSE;
	}

	if (fb_link->fbc_connection == NULL) {
		_php_fbird_module_error("Connection has no OO API handle");
		RETURN_FALSE;
	}

	attachment = fbc_get_attachment(fb_link->fbc_connection);
	if (attachment == NULL) {
		_php_fbird_module_error("Failed to get attachment from connection");
		RETURN_FALSE;
	}

	reconnected_trans = fbt_reconnect(FBG(master_instance), attachment, trans_id, IB_STATUS);
	if (reconnected_trans == NULL) {
		_php_fbird_error(IB_STATUS);
		RETURN_FALSE;
	}

	/* Allocate and initialize transaction structure */
	fb_trans = (fbird_transaction *)safe_emalloc(1, sizeof(fbird_transaction), 0);
	fb_trans->fbt_transaction = reconnected_trans;
	fb_trans->link_cnt = 1;
	fb_trans->affected_rows = 0;
	fb_trans->db_link[0] = fb_link;

	/* Link into connection's transaction list */
	if (fb_link->tr_list == NULL) {
		fb_link->tr_list = (fbird_tr_list *)emalloc(sizeof(fbird_tr_list));
		fb_link->tr_list->trans = NULL;
		fb_link->tr_list->next = NULL;
	}

	fbird_tr_list **l;
	for (l = &fb_link->tr_list; *l != NULL; l = &(*l)->next);
	*l = (fbird_tr_list *)emalloc(sizeof(fbird_tr_list));
	(*l)->trans = fb_trans;
	(*l)->next = NULL;

	zend_resource *res = zend_register_resource(fb_trans, le_trans);
	fbird_setup_transaction_object(return_value, res);
}

#endif /* HAVE_FIREBIRD */

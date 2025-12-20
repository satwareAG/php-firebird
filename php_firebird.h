/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */

#ifndef PHP_FIREBIRD_H
#define PHP_FIREBIRD_H

extern zend_module_entry firebird_module_entry;
#define phpext_firebird_ptr &firebird_module_entry

#include "ibase.h"

#ifndef FB_API_VER
  static_assert(false, "FATAL: FB_API_VER is not defined. Assumed very old, unsupported client library");
#endif

#define PHP_FIREBIRD_VER_MAJOR 1
#define PHP_FIREBIRD_VER_MINOR 0
#define PHP_FIREBIRD_VER_REV 0
/* #define PHP_FIREBIRD_VER_PRE "-RC2" -- Defined only for pre-releases */

// Keep two digit style similar to FB_API_VER
#define PHP_FIREBIRD_VER PHP_FIREBIRD_VER_MAJOR * 10 + PHP_FIREBIRD_VER_MINOR

#ifdef PHP_FIREBIRD_VER_PRE
#   define PHP_FIREBIRD_VER_STR "1.0.0" PHP_FIREBIRD_VER_PRE
#else
#   define PHP_FIREBIRD_VER_STR "1.0.0"
#endif

PHP_MINIT_FUNCTION(fbird);
PHP_RINIT_FUNCTION(fbird);
PHP_MSHUTDOWN_FUNCTION(fbird);
PHP_RSHUTDOWN_FUNCTION(fbird);
PHP_MINFO_FUNCTION(fbird);

PHP_FUNCTION(fbird_connect);
PHP_FUNCTION(fbird_pconnect);
PHP_FUNCTION(fbird_close);
PHP_FUNCTION(fbird_drop_db);
PHP_FUNCTION(fbird_query);
PHP_FUNCTION(fbird_fetch_row);
PHP_FUNCTION(fbird_fetch_assoc);
PHP_FUNCTION(fbird_fetch_object);
PHP_FUNCTION(fbird_free_result);
PHP_FUNCTION(fbird_name_result);
PHP_FUNCTION(fbird_prepare);
PHP_FUNCTION(fbird_execute);
PHP_FUNCTION(fbird_free_query);

PHP_FUNCTION(fbird_execute_statement);
PHP_FUNCTION(fbird_execute_query);
PHP_FUNCTION(fbird_execute_auto);

PHP_FUNCTION(fbird_timefmt);

PHP_FUNCTION(fbird_gen_id);
PHP_FUNCTION(fbird_num_fields);
PHP_FUNCTION(fbird_num_params);
PHP_FUNCTION(fbird_affected_rows);
PHP_FUNCTION(fbird_field_info);
PHP_FUNCTION(fbird_param_info);

PHP_FUNCTION(fbird_trans);
PHP_FUNCTION(fbird_trans_start);
PHP_FUNCTION(fbird_commit);
PHP_FUNCTION(fbird_rollback);
PHP_FUNCTION(fbird_commit_ret);
PHP_FUNCTION(fbird_rollback_ret);

PHP_FUNCTION(fbird_savepoint);
PHP_FUNCTION(fbird_rollback_savepoint);
PHP_FUNCTION(fbird_release_savepoint);

PHP_FUNCTION(fbird_trans_info);
PHP_FUNCTION(fbird_connection_info);

PHP_FUNCTION(fbird_blob_create);
PHP_FUNCTION(fbird_blob_add);
PHP_FUNCTION(fbird_blob_cancel);
PHP_FUNCTION(fbird_blob_open);
PHP_FUNCTION(fbird_blob_get);
PHP_FUNCTION(fbird_blob_close);
PHP_FUNCTION(fbird_blob_echo);
PHP_FUNCTION(fbird_blob_info);
PHP_FUNCTION(fbird_blob_import);
PHP_FUNCTION(fbird_blob_create_stream);
PHP_FUNCTION(fbird_blob_open_stream);
PHP_FUNCTION(fbird_blob_create_seekable);
PHP_FUNCTION(fbird_blob_open_seekable);
PHP_FUNCTION(fbird_blob_seek);

PHP_FUNCTION(fbird_add_user);
PHP_FUNCTION(fbird_modify_user);
PHP_FUNCTION(fbird_delete_user);

PHP_FUNCTION(fbird_service_attach);
PHP_FUNCTION(fbird_service_detach);
PHP_FUNCTION(fbird_backup);
PHP_FUNCTION(fbird_restore);
PHP_FUNCTION(fbird_maintain_db);
PHP_FUNCTION(fbird_db_info);
PHP_FUNCTION(fbird_server_info);

PHP_FUNCTION(fbird_errmsg);
PHP_FUNCTION(fbird_errcode);
PHP_FUNCTION(fbird_sqlstate);

PHP_FUNCTION(fbird_wait_event);
PHP_FUNCTION(fbird_set_event_handler);
PHP_FUNCTION(fbird_poll_event);
PHP_FUNCTION(fbird_free_event_handler);

PHP_FUNCTION(fbird_get_client_version);
PHP_FUNCTION(fbird_get_client_major_version);
PHP_FUNCTION(fbird_get_client_minor_version);

#else

#define phpext_firebird_ptr NULL

#endif /* PHP_FIREBIRD_H */

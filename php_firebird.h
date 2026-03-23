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

/* Version string: defined by configure script via AC_DEFINE_UNQUOTED.
 * Fallback for manual builds or missing configure detection. */
#ifndef PHP_FIREBIRD_VERSION_STRING
#  define PHP_FIREBIRD_VERSION_STRING "0.0.0-unknown"
#endif

/* Legacy compatibility macro (use PHP_FIREBIRD_VERSION_STRING in new code) */
#define PHP_FIREBIRD_VER_STR PHP_FIREBIRD_VERSION_STRING

/* Numeric version for the FBIRD_VER constant (two-digit style like FB_API_VER).
 * This is a simplified value; for full version info use PHP_FIREBIRD_VERSION_STRING. */
#ifndef PHP_FIREBIRD_VER
#  define PHP_FIREBIRD_VER 70  /* 7.x series */
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
PHP_FUNCTION(fbird_create_database);
PHP_FUNCTION(fbird_prepare_ex);
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
PHP_FUNCTION(fbird_query_params_tx);

PHP_FUNCTION(fbird_timefmt);

PHP_FUNCTION(fbird_gen_id);
PHP_FUNCTION(fbird_last_insert_id);
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

PHP_FUNCTION(fbird_escape_string);

/* Exception Mode API (PDO-style error handling) */
PHP_FUNCTION(fbird_set_exception_mode);
PHP_FUNCTION(fbird_get_exception_mode);

/* Exception mode constants */
#define FBIRD_EXCEPTION_MODE_SILENT 0
#define FBIRD_EXCEPTION_MODE_THROW  1

PHP_FUNCTION(fbird_wait_event);
PHP_FUNCTION(fbird_set_event_handler);
PHP_FUNCTION(fbird_poll_event);
PHP_FUNCTION(fbird_free_event_handler);

PHP_FUNCTION(fbird_get_client_version);
PHP_FUNCTION(fbird_get_client_major_version);
PHP_FUNCTION(fbird_get_client_minor_version);

/* Limbo Transaction Functions (Two-Phase Commit Recovery) */
PHP_FUNCTION(fbird_get_limbo_transactions);
PHP_FUNCTION(fbird_reconnect_transaction);

/* IBatch API Functions (Firebird 4.0+ Bulk Operations) */
#if FB_API_VER >= 40
PHP_FUNCTION(fbird_batch_create);
PHP_FUNCTION(fbird_batch_add);
PHP_FUNCTION(fbird_batch_execute);
PHP_FUNCTION(fbird_batch_cancel);
PHP_FUNCTION(fbird_batch_add_blob);
PHP_FUNCTION(fbird_batch_register_blob);
PHP_FUNCTION(fbird_batch_get_blob_alignment);
PHP_FUNCTION(fbird_batch_append_blob_data);
PHP_FUNCTION(fbird_batch_add_blob_stream);
PHP_FUNCTION(fbird_batch_set_default_bpb);
#endif /* FB_API_VER >= 40 */

#else

#define phpext_firebird_ptr NULL

#endif /* PHP_FIREBIRD_H */
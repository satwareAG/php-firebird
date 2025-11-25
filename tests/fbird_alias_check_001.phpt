--TEST--
Check for existence of fbird_* aliases
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
$aliases = [
    'fbird_connect',
    'fbird_pconnect',
    'fbird_close',
    'fbird_drop_db',
    'fbird_query',
    'fbird_fetch_row',
    'fbird_fetch_assoc',
    'fbird_fetch_object',
    'fbird_free_result',
    'fbird_name_result',
    'fbird_prepare',
    'fbird_execute',
    'fbird_free_query',
    'fbird_gen_id',
    'fbird_num_fields',
    'fbird_num_params',
    'fbird_affected_rows',
    'fbird_field_info',
    'fbird_param_info',
    'fbird_trans',
    'fbird_commit',
    'fbird_rollback',
    'fbird_commit_ret',
    'fbird_rollback_ret',
    'fbird_blob_info',
    'fbird_blob_create',
    'fbird_blob_add',
    'fbird_blob_cancel',
    'fbird_blob_close',
    'fbird_blob_open',
    'fbird_blob_get',
    'fbird_blob_echo',
    'fbird_blob_import',
    'fbird_errmsg',
    'fbird_errcode',
    'fbird_add_user',
    'fbird_modify_user',
    'fbird_delete_user',
    'fbird_service_attach',
    'fbird_service_detach',
    'fbird_backup',
    'fbird_restore',
    'fbird_maintain_db',
    'fbird_db_info',
    'fbird_server_info',
    'fbird_wait_event',
    'fbird_set_event_handler',
    'fbird_free_event_handler',
    'fbird_get_client_version',
    'fbird_get_client_major_version',
    'fbird_get_client_minor_version'
];

foreach ($aliases as $alias) {
    if (function_exists($alias)) {
        echo "$alias exists\n";
    } else {
        echo "$alias missing\n";
    }
}
?>
--EXPECT--
fbird_connect exists
fbird_pconnect exists
fbird_close exists
fbird_drop_db exists
fbird_query exists
fbird_fetch_row exists
fbird_fetch_assoc exists
fbird_fetch_object exists
fbird_free_result exists
fbird_name_result exists
fbird_prepare exists
fbird_execute exists
fbird_free_query exists
fbird_gen_id exists
fbird_num_fields exists
fbird_num_params exists
fbird_affected_rows exists
fbird_field_info exists
fbird_param_info exists
fbird_trans exists
fbird_commit exists
fbird_rollback exists
fbird_commit_ret exists
fbird_rollback_ret exists
fbird_blob_info exists
fbird_blob_create exists
fbird_blob_add exists
fbird_blob_cancel exists
fbird_blob_close exists
fbird_blob_open exists
fbird_blob_get exists
fbird_blob_echo exists
fbird_blob_import exists
fbird_errmsg exists
fbird_errcode exists
fbird_add_user exists
fbird_modify_user exists
fbird_delete_user exists
fbird_service_attach exists
fbird_service_detach exists
fbird_backup exists
fbird_restore exists
fbird_maintain_db exists
fbird_db_info exists
fbird_server_info exists
fbird_wait_event exists
fbird_set_event_handler exists
fbird_free_event_handler exists
fbird_get_client_version exists
fbird_get_client_major_version exists
fbird_get_client_minor_version exists

<?php

/**
 * PHPStan stub file for Firebird extension functions
 *
 * This file provides type hints for static analysis tools (PHPStan, IDEs)
 * without affecting runtime behavior.
 */

// Connection Management
function fbird_connect(string $database, string $username = '', string $password = '', string $charset = '', int $buffers = 0, int $dialect = 3, string $role = '', int $sync = 0): resource|false {}
function fbird_pconnect(string $database, string $username = '', string $password = '', string $charset = '', int $buffers = 0, int $dialect = 3, string $role = '', int $sync = 0): resource|false {}
function fbird_close(resource $connection = null): bool {}
function fbird_drop_db(resource $connection = null): bool {}

// Transaction Management
function fbird_trans(int $trans_args = 0, resource $link_identifier = null): resource|false {}
function fbird_commit(resource $link_or_trans_identifier = null): bool {}
function fbird_rollback(resource $link_or_trans_identifier = null): bool {}
function fbird_commit_ret(resource $link_or_trans_identifier = null): bool {}
function fbird_rollback_ret(resource $link_or_trans_identifier = null): bool {}

// Query Execution
function fbird_query(resource $link_identifier = null, string $query = '', int $bind_args = 0): resource|false {}
function fbird_prepare(resource $link_identifier = null, string $query = ''): resource|false {}
function fbird_execute(resource $query, mixed ...$bind_args): resource|false {}
function fbird_free_query(resource $query): bool {}
function fbird_free_result(resource $result): bool {}

// Result Fetching
function fbird_fetch_row(resource $result, int $fetch_flag = 0): array|false {}
function fbird_fetch_assoc(resource $result, int $fetch_flag = 0): array|false {}
function fbird_fetch_object(resource $result, int $fetch_flag = 0): object|false {}

// Field Information
function fbird_field_info(resource $result, int $field_number): array|false {}
function fbird_num_fields(resource $query_result): int|false {}

// Parameter Information
function fbird_param_info(resource $query, int $param_number): array|false {}
function fbird_num_params(resource $query): int|false {}

// Result Set Metadata
function fbird_affected_rows(resource $link_or_trans_identifier = null): int|false {}
function fbird_name_result(resource $result, int $index): string|false {}

// BLOB Handling
function fbird_blob_info(resource $link_or_trans_identifier, string $blob_id): array|false {}
function fbird_blob_add(resource $blob_handle, string $data): bool {}
function fbird_blob_cancel(resource $blob_handle): bool {}
function fbird_blob_close(resource $blob_handle): string|false {}
function fbird_blob_create(resource $link_identifier = null): resource|false {}
function fbird_blob_echo(resource $link_identifier, string $blob_id): bool {}
function fbird_blob_get(resource $blob_handle, int $len): string|false {}
function fbird_blob_import(resource $link_identifier, resource $file_handle): string|false {}
function fbird_blob_open(resource $link_identifier, string $blob_id): resource|false {}

// Batch Operations (Firebird 4.0+)
function fbird_batch_create(resource $link_identifier, string $query): resource|false {}
function fbird_batch_add(resource $batch, mixed ...$bind_args): bool {}
function fbird_batch_execute(resource $batch): bool {}
function fbird_batch_free(resource $batch): bool {}

// Service API
function fbird_service_attach(string $host, string $dba_username, string $dba_password): resource|false {}
function fbird_service_detach(resource $service_handle): bool {}
function fbird_backup(resource $service_handle, string $source_db, string $dest_file, int $options = 0, bool $verbose = false): mixed {}
function fbird_restore(resource $service_handle, string $source_file, string $dest_db, int $options = 0, bool $verbose = false): mixed {}
function fbird_maintain_db(resource $service_handle, string $db, int $action, int $argument = 0): bool {}
function fbird_db_info(resource $service_handle, string $db, int $action, int $argument = 0): string {}
function fbird_server_info(resource $service_handle, int $action): string {}
function fbird_wait_event(resource $link_identifier, string ...$event_names): string {}
function fbird_set_event_handler(resource $link_identifier, callable $event_handler, string ...$event_names): resource|false {}
function fbird_free_event_handler(resource $event): bool {}

// Error Handling
function fbird_errmsg(): string|false {}
function fbird_errcode(): int|false {}
function fbird_sqlstate(resource $link_or_trans_identifier): string|false {}

// Exception Mode API (PDO-style error handling)
function fbird_set_exception_mode(int $mode): bool {}
function fbird_get_exception_mode(): int {}

// Utility Functions
function fbird_gen_id(string $generator, int $increment = 1, resource $link_identifier = null): int|false {}

// Constants - Connection
const FBIRD_TEXT: int;
const FBIRD_FETCH_BLOBS: int;
const FBIRD_FETCH_ARRAYS: int;

// Constants - Transaction Isolation
const FBIRD_CONSISTENCY: int;
const FBIRD_CONCURRENCY: int;
const FBIRD_READ: int;
const FBIRD_WRITE: int;
const FBIRD_COMMITTED: int;
const FBIRD_WAIT: int;
const FBIRD_NOWAIT: int;

// Constants - Backup/Restore Options
const FBIRD_BKP_IGNORE_CHECKSUMS: int;
const FBIRD_BKP_IGNORE_LIMBO: int;
const FBIRD_BKP_METADATA_ONLY: int;
const FBIRD_BKP_NO_GARBAGE_COLLECT: int;
const FBIRD_BKP_OLD_DESCRIPTIONS: int;
const FBIRD_BKP_NON_TRANSPORTABLE: int;
const FBIRD_BKP_CONVERT: int;
const FBIRD_RES_DEACTIVATE_IDX: int;
const FBIRD_RES_NO_SHADOW: int;
const FBIRD_RES_NO_VALIDITY: int;
const FBIRD_RES_ONE_AT_A_TIME: int;
const FBIRD_RES_REPLACE: int;
const FBIRD_RES_CREATE: int;
const FBIRD_RES_USE_ALL_SPACE: int;

// Constants - Maintenance
const FBIRD_PRP_PAGE_BUFFERS: int;
const FBIRD_PRP_SWEEP_INTERVAL: int;
const FBIRD_PRP_SHUTDOWN_DB: int;
const FBIRD_PRP_DENY_NEW_TRANSACTIONS: int;
const FBIRD_PRP_DENY_NEW_ATTACHMENTS: int;
const FBIRD_PRP_RESERVE_SPACE: int;
const FBIRD_PRP_RES_USE_FULL: int;
const FBIRD_PRP_RES: int;
const FBIRD_PRP_WRITE_MODE: int;
const FBIRD_PRP_WM_ASYNC: int;
const FBIRD_PRP_WM_SYNC: int;
const FBIRD_PRP_ACTIVATE: int;
const FBIRD_PRP_DB_ONLINE: int;
const FBIRD_RPR_CHECK_DB: int;
const FBIRD_RPR_IGNORE_CHECKSUM: int;
const FBIRD_RPR_KILL_SHADOWS: int;
const FBIRD_RPR_MEND_DB: int;
const FBIRD_RPR_VALIDATE_DB: int;
const FBIRD_RPR_FULL: int;
const FBIRD_RPR_SWEEP_DB: int;

// Constants - Database Info
const FBIRD_STS_DATA_PAGES: int;
const FBIRD_STS_DB_LOG: int;
const FBIRD_STS_HDR_PAGES: int;
const FBIRD_STS_IDX_PAGES: int;
const FBIRD_STS_SYS_RELATIONS: int;

// Constants - Server Info
const FBIRD_SVC_SERVER_VERSION: int;
const FBIRD_SVC_IMPLEMENTATION: int;
const FBIRD_SVC_GET_ENV: int;
const FBIRD_SVC_GET_ENV_LOCK: int;
const FBIRD_SVC_GET_ENV_MSG: int;
const FBIRD_SVC_USER_DBPATH: int;
const FBIRD_SVC_SVR_DB_INFO: int;
const FBIRD_SVC_GET_USERS: int;

// Constants - Exception Mode
const FBIRD_EXCEPTION_MODE_SILENT: int;
const FBIRD_EXCEPTION_MODE_THROW: int;

// Firebird Exception Class
namespace Firebird {
    class Exception extends \Exception {
        /**
         * Get SQLSTATE error code
         *
         * @return string 5-character SQLSTATE code (e.g., "42000", "HY000")
         */
        public function getSqlState(): string {}
    }
}

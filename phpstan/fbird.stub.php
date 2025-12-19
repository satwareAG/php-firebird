<?php

/**
 * PHPStan stub file for php-firebird extension constants and functions.
 *
 * This file defines the constants and function signatures that are provided
 * by the C extension but are not visible to static analysis tools.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

// ============================================================================
// GENERAL CONSTANTS
// ============================================================================

/** Default/no special flags */
const FBIRD_DEFAULT = 0;

/** Create database flag */
const FBIRD_CREATE = 0;

/** Force new connection (bypass connection reuse) */
const FBIRD_CONNECT_FORCE_NEW = 2;

/** Extension version */
const FBIRD_VER = 10;

// ============================================================================
// FETCH FLAGS
// ============================================================================

/** Fetch BLOBs as strings instead of blob IDs */
const FBIRD_TEXT = 1;

/** Fetch BLOBs as strings (alias for FBIRD_TEXT) */
const FBIRD_FETCH_BLOBS = 1;

/** Fetch arrays as PHP arrays */
const FBIRD_FETCH_ARRAYS = 2;

/** Return timestamps as Unix timestamps */
const FBIRD_UNIXTIME = 4;

// ============================================================================
// TRANSACTION ACCESS MODES
// ============================================================================

/** Read-write transaction (default) */
const FBIRD_WRITE = 1;

/** Read-only transaction */
const FBIRD_READ = 2;

// ============================================================================
// TRANSACTION ISOLATION LEVELS
// ============================================================================

/** SNAPSHOT isolation (CONCURRENCY) */
const FBIRD_CONCURRENCY = 4;

/** READ COMMITTED isolation */
const FBIRD_COMMITTED = 8;

/** SNAPSHOT TABLE STABILITY isolation (CONSISTENCY) */
const FBIRD_CONSISTENCY = 16;

/** Read committed: use record version */
const FBIRD_REC_VERSION = 64;

/** Read committed: no record version */
const FBIRD_REC_NO_VERSION = 32;

/** Read committed with READ CONSISTENCY (Firebird 4.0+) */
const FBIRD_READ_CONSISTENCY = 32768;

// ============================================================================
// TRANSACTION LOCK RESOLUTION
// ============================================================================

/** Wait for lock resolution */
const FBIRD_WAIT = 128;

/** No wait - fail immediately on lock conflict */
const FBIRD_NOWAIT = 256;

/** Lock timeout flag (used with FBIRD_WAIT) */
const FBIRD_LOCK_TIMEOUT = 512;

// ============================================================================
// TABLE RESERVATION LOCK TYPES
// ============================================================================

/** Shared table lock */
const FBIRD_LOCK_SHARED = 1024;

/** Protected table lock */
const FBIRD_LOCK_PROTECTED = 2048;

/** Exclusive table lock */
const FBIRD_LOCK_EXCLUSIVE = 4096;

// ============================================================================
// TABLE RESERVATION ACCESS TYPES
// ============================================================================

/** Read access for table reservation */
const FBIRD_LOCK_READ = 8192;

/** Write access for table reservation */
const FBIRD_LOCK_WRITE = 16384;

// ============================================================================
// EVENT CONSTANTS
// ============================================================================

/** Event timeout return value */
const FBIRD_EVENT_TIMEOUT = -2;

// ============================================================================
// BLOB SEEK CONSTANTS
// ============================================================================

/** Seek from beginning of BLOB */
const FBIRD_BLOB_SEEK_SET = 0;

/** Seek from current position */
const FBIRD_BLOB_SEEK_CUR = 1;

/** Seek from end of BLOB */
const FBIRD_BLOB_SEEK_END = 2;

// ============================================================================
// SERVICE API CONSTANTS - BACKUP OPTIONS
// ============================================================================

const FBIRD_BKP_IGNORE_CHECKSUMS = 1;
const FBIRD_BKP_IGNORE_LIMBO = 2;
const FBIRD_BKP_METADATA_ONLY = 4;
const FBIRD_BKP_NO_GARBAGE_COLLECT = 8;
const FBIRD_BKP_OLD_DESCRIPTIONS = 16;
const FBIRD_BKP_NON_TRANSPORTABLE = 32;
const FBIRD_BKP_CONVERT = 64;

// ============================================================================
// SERVICE API CONSTANTS - RESTORE OPTIONS
// ============================================================================

const FBIRD_RES_DEACTIVATE_IDX = 256;
const FBIRD_RES_NO_SHADOW = 512;
const FBIRD_RES_NO_VALIDITY = 1024;
const FBIRD_RES_ONE_AT_A_TIME = 2048;
const FBIRD_RES_REPLACE = 4096;
const FBIRD_RES_CREATE = 8192;
const FBIRD_RES_USE_ALL_SPACE = 16384;

// ============================================================================
// SERVICE API CONSTANTS - DATABASE PROPERTIES
// ============================================================================

const FBIRD_PRP_PAGE_BUFFERS = 5;
const FBIRD_PRP_SWEEP_INTERVAL = 6;
const FBIRD_PRP_SHUTDOWN_DB = 7;
const FBIRD_PRP_DENY_NEW_TRANSACTIONS = 10;
const FBIRD_PRP_DENY_NEW_ATTACHMENTS = 9;
const FBIRD_PRP_RESERVE_SPACE = 11;
const FBIRD_PRP_RES_USE_FULL = 35;
const FBIRD_PRP_RES = 36;
const FBIRD_PRP_WRITE_MODE = 12;
const FBIRD_PRP_WM_ASYNC = 37;
const FBIRD_PRP_WM_SYNC = 38;
const FBIRD_PRP_ACCESS_MODE = 13;
const FBIRD_PRP_AM_READONLY = 39;
const FBIRD_PRP_AM_READWRITE = 40;
const FBIRD_PRP_SET_SQL_DIALECT = 14;
const FBIRD_PRP_ACTIVATE = 256;
const FBIRD_PRP_DB_ONLINE = 512;

// ============================================================================
// SERVICE API CONSTANTS - REPAIR OPTIONS
// ============================================================================

const FBIRD_RPR_CHECK_DB = 16;
const FBIRD_RPR_IGNORE_CHECKSUM = 32;
const FBIRD_RPR_KILL_SHADOWS = 64;
const FBIRD_RPR_MEND_DB = 4;
const FBIRD_RPR_VALIDATE_DB = 1;
const FBIRD_RPR_FULL = 128;
const FBIRD_RPR_SWEEP_DB = 2;

// ============================================================================
// SERVICE API CONSTANTS - STATISTICS
// ============================================================================

const FBIRD_STS_DATA_PAGES = 1;
const FBIRD_STS_DB_LOG = 2;
const FBIRD_STS_HDR_PAGES = 4;
const FBIRD_STS_IDX_PAGES = 8;
const FBIRD_STS_SYS_RELATIONS = 16;

// ============================================================================
// SERVICE API CONSTANTS - SERVER INFO
// ============================================================================

const FBIRD_SVC_SERVER_VERSION = 55;
const FBIRD_SVC_IMPLEMENTATION = 56;
const FBIRD_SVC_GET_ENV = 59;
const FBIRD_SVC_GET_ENV_LOCK = 60;
const FBIRD_SVC_GET_ENV_MSG = 61;
const FBIRD_SVC_USER_DBPATH = 58;
const FBIRD_SVC_SVR_DB_INFO = 50;
const FBIRD_SVC_GET_USERS = 68;

// ============================================================================
// CONNECTION FUNCTIONS
// ============================================================================

/**
 * @param string $database
 * @param string|null $username
 * @param string|null $password
 * @param string|null $charset
 * @param int $buffers
 * @param int $dialect
 * @param string|null $role
 * @param int $flags
 * @return resource|false
 */
function fbird_connect(
    string $database,
    ?string $username = null,
    ?string $password = null,
    ?string $charset = null,
    int $buffers = 0,
    int $dialect = 3,
    ?string $role = null,
    int $flags = 0
): mixed {}

/**
 * @param string $database
 * @param string|null $username
 * @param string|null $password
 * @param string|null $charset
 * @param int $buffers
 * @param int $dialect
 * @param string|null $role
 * @return resource|false
 */
function fbird_pconnect(
    string $database,
    ?string $username = null,
    ?string $password = null,
    ?string $charset = null,
    int $buffers = 0,
    int $dialect = 3,
    ?string $role = null
): mixed {}

/**
 * @param resource|null $connection
 * @return bool
 */
function fbird_close(mixed $connection = null): bool {}

/**
 * @param resource|null $connection
 * @return bool
 */
function fbird_drop_db(mixed $connection = null): bool {}

// ============================================================================
// QUERY FUNCTIONS
// ============================================================================

/**
 * @param resource|string $link_or_query
 * @param mixed ...$args
 * @return resource|bool
 */
function fbird_query(mixed $link_or_query, mixed ...$args): mixed {}

/**
 * @param resource|string $link_or_query
 * @param mixed ...$args
 * @return resource|false
 */
function fbird_prepare(mixed $link_or_query, mixed ...$args): mixed {}

/**
 * @param resource $query
 * @param mixed ...$bind_args
 * @return resource|bool
 */
function fbird_execute(mixed $query, mixed ...$bind_args): mixed {}

/**
 * @param resource $query
 * @return bool
 */
function fbird_free_query(mixed $query): bool {}

/**
 * @param resource $result
 * @return bool
 */
function fbird_free_result(mixed $result): bool {}

// ============================================================================
// FETCH FUNCTIONS
// ============================================================================

/**
 * @param resource $result
 * @param int $fetch_flags
 * @return array<int, mixed>|false
 */
function fbird_fetch_row(mixed $result, int $fetch_flags = 0): array|false {}

/**
 * @param resource $result
 * @param int $fetch_flags
 * @return array<string, mixed>|false
 */
function fbird_fetch_assoc(mixed $result, int $fetch_flags = 0): array|false {}

/**
 * @param resource $result
 * @param int $fetch_flags
 * @return object|false
 */
function fbird_fetch_object(mixed $result, int $fetch_flags = 0): object|false {}

/**
 * @param resource $result
 * @param string $name
 * @return bool
 */
function fbird_name_result(mixed $result, string $name): bool {}

// ============================================================================
// FIELD/PARAMETER INFO FUNCTIONS
// ============================================================================

/**
 * @param resource $result
 * @return int|false
 */
function fbird_num_fields(mixed $result): int|false {}

/**
 * @param resource $query
 * @return int|false
 */
function fbird_num_params(mixed $query): int|false {}

/**
 * @param resource|null $link
 * @return int
 */
function fbird_affected_rows(mixed $link = null): int {}

/**
 * @param resource $result
 * @param int $field_number
 * @return array<string, mixed>|false
 */
function fbird_field_info(mixed $result, int $field_number): array|false {}

/**
 * @param resource $query
 * @param int $param_number
 * @return array<string, mixed>|false
 */
function fbird_param_info(mixed $query, int $param_number): array|false {}

// ============================================================================
// TRANSACTION FUNCTIONS
// ============================================================================

/**
 * @param resource|int|null $link_or_flags
 * @param mixed ...$args
 * @return resource|false
 */
function fbird_trans(mixed $link_or_flags = null, mixed ...$args): mixed {}

/**
 * @param resource $link
 * @param array<string, mixed>|int $options
 * @return resource|false
 */
function fbird_trans_start(mixed $link, array|int $options = FBIRD_DEFAULT): mixed {}

/**
 * @param resource|null $link
 * @return bool
 */
function fbird_commit(mixed $link = null): bool {}

/**
 * @param resource|null $link
 * @return bool
 */
function fbird_rollback(mixed $link = null): bool {}

/**
 * @param resource|null $link
 * @return bool
 */
function fbird_commit_ret(mixed $link = null): bool {}

/**
 * @param resource|null $link
 * @return bool
 */
function fbird_rollback_ret(mixed $link = null): bool {}

/**
 * @param resource $link
 * @param string $name
 * @return bool
 */
function fbird_savepoint(mixed $link, string $name): bool {}

/**
 * @param resource $link
 * @param string $name
 * @return bool
 */
function fbird_rollback_savepoint(mixed $link, string $name): bool {}

/**
 * @param resource $link
 * @param string $name
 * @return bool
 */
function fbird_release_savepoint(mixed $link, string $name): bool {}

/**
 * @param resource $link
 * @param int $req_items
 * @return array<string, mixed>|false
 */
function fbird_trans_info(mixed $link, int $req_items): array|false {}

// ============================================================================
// BLOB FUNCTIONS
// ============================================================================

/**
 * @param resource|null $link
 * @param resource|null $trans
 * @return resource|false
 */
function fbird_blob_create(mixed $link = null, mixed $trans = null): mixed {}

/**
 * @param resource $blob
 * @param string $data
 * @return bool
 */
function fbird_blob_add(mixed $blob, string $data): bool {}

/**
 * @param resource $blob
 * @return string|false
 */
function fbird_blob_close(mixed $blob): string|false {}

/**
 * @param resource $blob
 * @return bool
 */
function fbird_blob_cancel(mixed $blob): bool {}

/**
 * @param resource|string $link_or_id
 * @param mixed ...$args
 * @return resource|false
 */
function fbird_blob_open(mixed $link_or_id, mixed ...$args): mixed {}

/**
 * @param resource $blob
 * @param int $length
 * @return string|false
 */
function fbird_blob_get(mixed $blob, int $length): string|false {}

/**
 * @param resource|string $link_or_id
 * @param string|null $blob_id
 * @return bool
 */
function fbird_blob_echo(mixed $link_or_id, ?string $blob_id = null): bool {}

/**
 * @param resource|string $link_or_id
 * @param string|null $blob_id
 * @return array<string, mixed>|false
 */
function fbird_blob_info(mixed $link_or_id, ?string $blob_id = null): array|false {}

/**
 * @param resource $link
 * @param resource $file
 * @return string|false
 */
function fbird_blob_import(mixed $link, mixed $file): string|false {}

/**
 * @param resource|null $link
 * @return resource|false
 */
function fbird_blob_create_stream(mixed $link = null): mixed {}

/**
 * @param resource|string $link_or_id
 * @param string|null $blob_id
 * @return resource|false
 */
function fbird_blob_open_stream(mixed $link_or_id, ?string $blob_id = null): mixed {}

/**
 * @param resource|null $link
 * @return resource|false
 */
function fbird_blob_create_seekable(mixed $link = null): mixed {}

/**
 * @param resource|string $link_or_id
 * @param string|null $blob_id
 * @return resource|false
 */
function fbird_blob_open_seekable(mixed $link_or_id, ?string $blob_id = null): mixed {}

/**
 * @param resource $blob
 * @param int $offset
 * @param int $whence
 * @return int|false
 */
function fbird_blob_seek(mixed $blob, int $offset, int $whence = FBIRD_BLOB_SEEK_SET): int|false {}

// ============================================================================
// GENERATOR FUNCTIONS
// ============================================================================

/**
 * @param string $generator
 * @param int $increment
 * @param resource|null $link
 * @return int|string|false
 */
function fbird_gen_id(string $generator, int $increment = 1, mixed $link = null): int|string|false {}

// ============================================================================
// ERROR FUNCTIONS
// ============================================================================

/**
 * @return string
 */
function fbird_errmsg(): string {}

/**
 * @return int
 */
function fbird_errcode(): int {}

// ============================================================================
// EVENT FUNCTIONS
// ============================================================================

/**
 * @param resource|string $link_or_event
 * @param string ...$events
 * @return string|false
 */
function fbird_wait_event(mixed $link_or_event, string ...$events): string|false {}

/**
 * @param resource|callable $link_or_callback
 * @param callable|string $callback_or_event
 * @param string ...$events
 * @return resource|false
 */
function fbird_set_event_handler(mixed $link_or_callback, mixed $callback_or_event, string ...$events): mixed {}

/**
 * @param resource $event
 * @return array<string, int>|int|false
 */
function fbird_poll_event(mixed $event): array|int|false {}

/**
 * @param resource $event
 * @return bool
 */
function fbird_free_event_handler(mixed $event): bool {}

// ============================================================================
// SERVICE MANAGER FUNCTIONS
// ============================================================================

/**
 * @param string $host
 * @param string $username
 * @param string $password
 * @return resource|false
 */
function fbird_service_attach(string $host, string $username, string $password): mixed {}

/**
 * @param resource $service
 * @return bool
 */
function fbird_service_detach(mixed $service): bool {}

/**
 * @param resource $service
 * @param string $source
 * @param string $dest
 * @param int $options
 * @param bool $verbose
 * @return mixed
 */
function fbird_backup(mixed $service, string $source, string $dest, int $options = 0, bool $verbose = false): mixed {}

/**
 * @param resource $service
 * @param string $source
 * @param string $dest
 * @param int $options
 * @param bool $verbose
 * @return mixed
 */
function fbird_restore(mixed $service, string $source, string $dest, int $options = 0, bool $verbose = false): mixed {}

/**
 * @param resource $service
 * @param string $db
 * @param int $action
 * @param int $argument
 * @return bool
 */
function fbird_maintain_db(mixed $service, string $db, int $action, int $argument = 0): bool {}

/**
 * @param resource $service
 * @param string $db
 * @param int $action
 * @param int $argument
 * @return string|false
 */
function fbird_db_info(mixed $service, string $db, int $action, int $argument = 0): string|false {}

/**
 * @param resource $service
 * @param int $action
 * @return string|false
 */
function fbird_server_info(mixed $service, int $action): string|false {}

// ============================================================================
// USER MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * @param resource $service
 * @param string $username
 * @param string $password
 * @param string|null $first_name
 * @param string|null $middle_name
 * @param string|null $last_name
 * @return bool
 */
function fbird_add_user(
    mixed $service,
    string $username,
    string $password,
    ?string $first_name = null,
    ?string $middle_name = null,
    ?string $last_name = null
): bool {}

/**
 * @param resource $service
 * @param string $username
 * @param string $password
 * @param string|null $first_name
 * @param string|null $middle_name
 * @param string|null $last_name
 * @return bool
 */
function fbird_modify_user(
    mixed $service,
    string $username,
    string $password,
    ?string $first_name = null,
    ?string $middle_name = null,
    ?string $last_name = null
): bool {}

/**
 * @param resource $service
 * @param string $username
 * @return bool
 */
function fbird_delete_user(mixed $service, string $username): bool {}

// ============================================================================
// VERSION FUNCTIONS
// ============================================================================

/**
 * @return string
 */
function fbird_get_client_version(): string {}

/**
 * @return int
 */
function fbird_get_client_major_version(): int {}

/**
 * @return int
 */
function fbird_get_client_minor_version(): int {}

// ============================================================================
// TIME FORMAT FUNCTION
// ============================================================================

/**
 * @param string $format
 * @param int $type
 * @return bool
 */
function fbird_timefmt(string $format, int $type = 0): bool {}

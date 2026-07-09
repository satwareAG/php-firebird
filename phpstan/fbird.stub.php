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

/** Extension version (100 for 10.x+ series) */
const FBIRD_VER = 100;

// ============================================================================
// EXCEPTION MODE CONSTANTS
// ============================================================================

/** Exception mode: suppress errors (default) */
const FBIRD_EXCEPTION_MODE_SILENT = 0;

/** Exception mode: throw exceptions on errors */
const FBIRD_EXCEPTION_MODE_THROW = 1;

/** Exception mode: backward-compatible alias for SILENT (default) */
const FBIRD_EXCEPTION_MODE_COMPAT = 0;

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

/** Return date/time columns as DateTimeImmutable objects */
const FBIRD_FETCH_DATE_OBJ = 8;

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
 * @return \Firebird\Connection|false
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
): \Firebird\Connection|false {}

/**
 * @param string $database
 * @param string|null $username
 * @param string|null $password
 * @param string|null $charset
 * @param int $buffers
 * @param int $dialect
 * @param string|null $role
 * @return \Firebird\Connection|false
 */
function fbird_pconnect(
    string $database,
    ?string $username = null,
    ?string $password = null,
    ?string $charset = null,
    int $buffers = 0,
    int $dialect = 3,
    ?string $role = null
): \Firebird\Connection|false {}

/**
 * @param \Firebird\Connection|null $connection
 * @return bool
 */
function fbird_close(mixed $connection = null): bool {}

/**
 * @param \Firebird\Connection|null $connection
 * @return bool
 */
function fbird_drop_db(mixed $connection = null): bool {}

/**
 * @param string $database Database connection string
 * @param string|null $username
 * @param string|null $password
 * @param string|null $charset
 * @param int|null $page_size
 * @return \Firebird\Connection|false
 */
function fbird_create_database(
    string $database,
    ?string $username = null,
    ?string $password = null,
    ?string $charset = null,
    ?int $page_size = null
): \Firebird\Connection|false {}

/**
 * @param mixed $link_identifier Connection resource
 * @param string $sequence Generator/sequence name
 * @return int|false
 */
function fbird_last_insert_id(mixed $link_identifier, ?string $sequence = null): int|false {}

// ============================================================================
// QUERY FUNCTIONS
// ============================================================================

/**
 * @param mixed $link_or_query
 * @param mixed ...$args
 * @return \Firebird\ResultSet|int|bool
 */
function fbird_query(mixed $link_or_query, mixed ...$args): \Firebird\ResultSet|int|bool {}

/**
 * Prepare a SQL statement for later execution.
 *
 * Accepts 1-3 arguments:
 * - fbird_prepare(string $query)
 * - fbird_prepare(resource $link, string $query)
 * - fbird_prepare(resource $link, resource $trans, string $query)
 * - fbird_prepare(resource $trans, string $query)
 * - fbird_prepare(resource $trans, resource $link, string $query)
 *
 * @param mixed $link_or_trans_or_query Link, transaction, or query string
 * @param mixed $link_or_trans_or_query_2 Link, transaction, or query string
 * @param string|null $query Query string when first two args are resources
 * @return \Firebird\Statement|false
 */
function fbird_prepare(
    mixed $link_or_trans_or_query,
    mixed $link_or_trans_or_query_2 = null,
    ?string $query = null
): \Firebird\Statement|false {}

/**
 * Prepare with fixed signature: connection, query, optional transaction.
 * @param mixed $link_identifier
 * @param string $query
 * @param mixed $trans_handle
 * @return \Firebird\Statement|false
 */
function fbird_prepare_ex(
    mixed $link_identifier,
    string $query,
    mixed $trans_handle = null
): \Firebird\Statement|false {}

/**
 * @param resource|\Firebird\ResultSet $query
 * @param mixed ...$bind_args
 * @return \Firebird\ResultSet|int|bool
 */
function fbird_execute(mixed $query, mixed ...$bind_args): \Firebird\ResultSet|int|bool {}

/**
 * @param mixed $trans_handle
 * @param string $query
 * @param array<int, mixed>|null $params
 * @return int|false
 */
function fbird_execute_statement(mixed $trans_handle, string $query, ?array $params = null): int|false {}

/**
 * @param mixed $trans_handle
 * @param string $query
 * @param array<int, mixed>|null $params
 * @return \Firebird\ResultSet|false
 */
function fbird_execute_query(mixed $trans_handle, string $query, ?array $params = null): \Firebird\ResultSet|false {}

/**
 * @param mixed $link_identifier
 * @param string $query
 * @param array<int, mixed>|null $params
 * @return \Firebird\ResultSet|int|false
 */
function fbird_execute_auto(mixed $link_identifier, string $query, ?array $params = null): \Firebird\ResultSet|int|false {}

/**
 * Execute a parameterized query with explicit link and transaction.
 * @param mixed $link_identifier Connection resource
 * @param \Firebird\Transaction|resource $trans_handle Transaction object or legacy resource
 * @param string $query SQL statement
 * @param array<mixed>|null $params Bind parameters
 * @return \Firebird\ResultSet|int|false
 * @since 7.1.0
 */
function fbird_query_params_tx(mixed $link_identifier, mixed $trans_handle, string $query, ?array $params = null): \Firebird\ResultSet|int|false {}

/**
 * @param mixed $query
 * @return bool
 */
function fbird_free_query(mixed $query): bool {}

/**
 * @param mixed $result
 * @return bool
 */
function fbird_free_result(mixed $result): bool {}

// ============================================================================
// FETCH FUNCTIONS
// ============================================================================

/**
 * @param mixed $result
 * @param int $fetch_flags
 * @return array<int, mixed>|false
 */
function fbird_fetch_row(mixed $result, int $fetch_flags = 0): array|false {}

/**
 * @param mixed $result
 * @param int $fetch_flags
 * @return array<string, mixed>|false
 */
function fbird_fetch_assoc(mixed $result, int $fetch_flags = 0): array|false {}

/**
 * @param mixed $result
 * @param int $fetch_flags
 * @return object|false
 */
function fbird_fetch_object(mixed $result, int $fetch_flags = 0): object|false {}

/**
 * @param mixed $result
 * @param string $name
 * @return bool
 */
function fbird_name_result(mixed $result, string $name): bool {}

// ============================================================================
// FIELD/PARAMETER INFO FUNCTIONS
// ============================================================================

/**
 * @param mixed $result
 * @return int|false
 */
function fbird_num_fields(mixed $result): int|false {}

/**
 * @param mixed $query
 * @return int|false
 */
function fbird_num_params(mixed $query): int|false {}

/**
 * @param mixed $link
 * @return int
 */
function fbird_affected_rows(mixed $link = null): int {}

/**
 * @param mixed $result
 * @param int $field_number
 * @return array<string, mixed>|false
 */
function fbird_field_info(mixed $result, int $field_number): array|false {}

/**
 * @param mixed $query
 * @param int $param_number
 * @return array<string, mixed>|false
 */
function fbird_param_info(mixed $query, int $param_number): array|false {}

// ============================================================================
// TRANSACTION FUNCTIONS
// ============================================================================

/**
 * @param mixed $link_or_flags
 * @param mixed ...$args
 * @return \Firebird\Transaction|false
 */
function fbird_trans(mixed $link_or_flags = null, mixed ...$args): \Firebird\Transaction|false {}

/**
 * Start a transaction with options.
 *
 * Legacy form (int bitmask):
 *   fbird_trans_start($link, FBIRD_DEFAULT | FBIRD_READ | ...)
 *
 * New form (options array) as used by `Firebird\TBuilder::build()`:
 *   fbird_trans_start($link, ['readCommitted' => true, 'lockTimeout' => 5, ...])
 *
 * @param mixed $link Database connection
 * @param array<string, array<string, int>|bool|int>|null $options Transaction options
 * @return \Firebird\Transaction|false Transaction handle or false on error
 */
function fbird_trans_start(mixed $link, ?array $options = null): \Firebird\Transaction|false {}

/**
 * @param mixed $link
 * @return bool
 */
function fbird_commit(mixed $link = null): bool {}

/**
 * @param mixed $link
 * @return bool
 */
function fbird_rollback(mixed $link = null): bool {}

/**
 * @param mixed $link
 * @return bool
 */
function fbird_commit_ret(mixed $link = null): bool {}

/**
 * @param mixed $link
 * @return bool
 */
function fbird_rollback_ret(mixed $link = null): bool {}

/**
 * @param mixed $link
 * @param string $name
 * @return bool
 */
function fbird_savepoint(mixed $link, string $name): bool {}

/**
 * @param mixed $link
 * @param string $name
 * @return bool
 */
function fbird_rollback_savepoint(mixed $link, string $name): bool {}

/**
 * @param mixed $link
 * @param string $name
 * @return bool
 */
function fbird_release_savepoint(mixed $link, string $name): bool {}

/**
 * Get transaction information.
 *
 * @param mixed $trans_handle Transaction resource
 * @return array<string, mixed>|false Transaction info array or false on failure
 */
function fbird_trans_info(mixed $trans_handle): array|false {}

// ============================================================================
// BLOB FUNCTIONS
// ============================================================================

/**
 * Create a blob for adding data.
 *
 * @param resource|\Firebird\Connection|null $link Database connection (optional, uses default)
 * @return \Firebird\Blob|false Blob handle or false on error
 */
function fbird_blob_create(mixed $link = null): \Firebird\Blob|false {}

/**
 * @param mixed $blob
 * @param string $data
 * @return bool
 */
function fbird_blob_add(mixed $blob, string $data): bool {}

/**
 * @param mixed $blob
 * @return string|false
 */
function fbird_blob_close(mixed $blob): string|false {}

/**
 * @param mixed $blob
 * @return bool
 */
function fbird_blob_cancel(mixed $blob): bool {}

/**
 * Open a blob for retrieving data.
 *
 * Accepts 1-2 arguments:
 * - fbird_blob_open(string $blob_id)
 * - fbird_blob_open(resource $link, string $blob_id)
 *
 * @param resource|\Firebird\Connection|string $link_or_blob_id Database connection or blob ID string
 * @param string|null $blob_id Blob ID string when first arg is a connection
 * @return \Firebird\Blob|false Blob handle or false on error
 */
function fbird_blob_open(mixed $link_or_blob_id, ?string $blob_id = null): \Firebird\Blob|false {}

/**
 * @param mixed $blob
 * @param int $length
 * @return string|false
 */
function fbird_blob_get(mixed $blob, int $length): string|false {}

/**
 * @param mixed $link_or_id
 * @param string|null $blob_id
 * @return bool
 */
function fbird_blob_echo(mixed $link_or_id, ?string $blob_id = null): bool {}

/**
 * @param mixed $link_or_id
 * @param string|null $blob_id
 * @return array<string, mixed>|false
 */
function fbird_blob_info(mixed $link_or_id, ?string $blob_id = null): array|false {}

/**
 * @param mixed $link
 * @param mixed $file
 * @return string|false
 */
function fbird_blob_import(mixed $link, mixed $file): string|false {}

/**
 * @param mixed $link
 * @return resource|false
 */
function fbird_blob_create_stream(mixed $link = null): mixed {}

/**
 * @param mixed $link_or_id
 * @param string|null $blob_id
 * @return resource|false
 */
function fbird_blob_open_stream(mixed $link_or_id, ?string $blob_id = null): mixed {}

/**
 * @param resource|\Firebird\Connection|null $link
 * @return \Firebird\Blob|false
 */
function fbird_blob_create_seekable(mixed $link = null): \Firebird\Blob|false {}

/**
 * @param resource|\Firebird\Connection|string $link_or_id
 * @param string|null $blob_id
 * @return \Firebird\Blob|false
 */
function fbird_blob_open_seekable(mixed $link_or_id, ?string $blob_id = null): \Firebird\Blob|false {}

/**
 * @param mixed $blob
 * @param int $offset
 * @param int $whence
 * @return int|false
 */
function fbird_blob_seek(mixed $blob, int $offset, int $whence = 0): int|false {}

// ============================================================================
// GENERATOR FUNCTIONS
// ============================================================================

/**
 * @param string $generator
 * @param int $increment
 * @param mixed $link
 * @return int|string|false
 */
function fbird_gen_id(string $generator, int $increment = 1, mixed $link = null): int|string|false {}

// ============================================================================
// ERROR FUNCTIONS
// ============================================================================

/**
 * Return error message
 */
function fbird_errmsg(): string|false {}

/**
 * Return error code
 */
function fbird_errcode(): int|false {}

/**
 * Return SQLSTATE error code for the last error
 *
 * Returns a 5-character SQLSTATE code (e.g., "23000" for integrity constraint
 * violation, "42000" for syntax error) based on the SQL:2003 standard.
 *
 * @return string|false The SQLSTATE code as a 5-character string, or false if no error
 */
function fbird_sqlstate(): string|false {}

/**
 * Escape a string for safe use in SQL queries.
 *
 * Escapes single quotes by doubling them (' ’ '').
 * Firebird SQL uses '' (two single quotes) as the escape sequence for
 * a literal single quote within string literals.
 *
 * Note: This function does NOT add surrounding quotes to the string.
 * You must still wrap the result in single quotes in your SQL.
 *
 * Example:
 *   $name = fbird_escape_string("O'Reilly");  // Returns: O''Reilly
 *   $sql = "SELECT * FROM users WHERE name = '$name'";
 *
 * @param string $string The string to escape
 * @return string The escaped string with single quotes doubled
 */
function fbird_escape_string(string $string): string {}

/**
 * Set the exception mode for error handling.
 *
 * @param int $mode FBIRD_EXCEPTION_MODE_SILENT (0) or FBIRD_EXCEPTION_MODE_THROW (1)
 * @return bool True on success
 */
function fbird_set_exception_mode(int $mode): bool {}

/**
 * Get the current exception mode.
 *
 * @return int Current mode: FBIRD_EXCEPTION_MODE_SILENT (0) or FBIRD_EXCEPTION_MODE_THROW (1)
 */
function fbird_get_exception_mode(): int {}

// ============================================================================
// EVENT FUNCTIONS
// ============================================================================

/**
 * @param mixed $link_or_event
 * @param string ...$events
 * @return string|false
 */
function fbird_wait_event(mixed $link_or_event, string ...$events): string|false {}

/**
 * @param resource|callable $link_or_callback
 * @param callable|string $callback_or_event
 * @param string ...$events
 * @return \Firebird\Event|false
 */
function fbird_set_event_handler(mixed $link_or_callback, mixed $callback_or_event, string ...$events): \Firebird\Event|false {}

/**
 * Poll for event occurrences (non-blocking).
 *
 * @param mixed $event Event handler resource
 * @param int $timeout_ms Timeout in milliseconds (default: 0 = non-blocking)
 * @return array<string, int>|int|false Event counts, timeout indicator, or false on error
 */
function fbird_poll_event(mixed $event, int $timeout_ms = 0): array|int|false {}

/**
 * @param mixed $event
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
 * @return \Firebird\Service|false
 */
function fbird_service_attach(string $host, string $username, string $password): \Firebird\Service|false {}

/**
 * @param \Firebird\Service $service
 * @return bool
 */
function fbird_service_detach(mixed $service): bool {}

/**
 * @param \Firebird\Service $service
 * @param string $source
 * @param string $dest
 * @param int $options
 * @param bool $verbose
 * @return mixed
 */
function fbird_backup(mixed $service, string $source, string $dest, int $options = 0, bool $verbose = false): mixed {}

/**
 * @param \Firebird\Service $service
 * @param string $source
 * @param string $dest
 * @param int $options
 * @param bool $verbose
 * @return mixed
 */
function fbird_restore(mixed $service, string $source, string $dest, int $options = 0, bool $verbose = false): mixed {}

/**
 * @param \Firebird\Service $service
 * @param string $db
 * @param int $action
 * @param int $argument
 * @return bool
 */
function fbird_maintain_db(mixed $service, string $db, int $action, int $argument = 0): bool {}

/**
 * @param \Firebird\Service $service
 * @param string $db
 * @param int $action
 * @param int $argument
 * @return string|false
 */
function fbird_db_info(mixed $service, string $db, int $action, int $argument = 0): string|false {}

/**
 * @param \Firebird\Service $service
 * @param int $action
 * @return string|false
 */
function fbird_server_info(mixed $service, int $action): string|false {}

// ============================================================================
// USER MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * @param \Firebird\Service $service
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
 * @param \Firebird\Service $service
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
 * @param \Firebird\Service $service
 * @param string $username
 * @return bool
 */
function fbird_delete_user(mixed $service, string $username): bool {}

// ============================================================================
// VERSION FUNCTIONS
// ============================================================================

/**
 * @return float
 */
function fbird_get_client_version(): float {}

/**
 * @return int
 */
function fbird_get_client_major_version(): int {}

/**
 * @return int
 */
function fbird_get_client_minor_version(): int {}

// ============================================================================
// CONNECTION INFO FUNCTIONS
// ============================================================================

/**
 * Get database connection statistics and information.
 *
 * Returns an associative array with database statistics and configuration:
 * - reads: Number of page reads
 * - writes: Number of page writes
 * - fetches: Number of fetches
 * - marks: Number of marks
 * - page_size: Database page size in bytes
 * - num_buffers: Number of database buffers
 * - current_memory: Current memory used by connection
 * - max_memory: Maximum memory used by connection
 * - allocation: Number of pages allocated
 * - attachment_id: Attachment identifier
 * - ods_version: On-Disk Structure major version
 * - ods_minor_version: On-Disk Structure minor version
 * - sql_dialect: SQL dialect in use
 *
 * @param mixed $link_identifier Database connection resource
 * @return array<string, int>|false Connection statistics array or false on error
 */
function fbird_connection_info(mixed $link_identifier = null): array|false {}

// ============================================================================
// LIMBO TRANSACTION FUNCTIONS (Two-Phase Commit Recovery)
// ============================================================================

/**
 * Get list of limbo (in-doubt) transaction IDs.
 *
 * @param mixed $link_identifier Database connection
 * @param int $max_count Maximum number of IDs to retrieve (1-10000)
 * @return array<int, int>|false Array of transaction IDs or false on error
 */
function fbird_get_limbo_transactions(mixed $link_identifier = null, int $max_count = 100): array|false {}

/**
 * Reconnect to a limbo transaction for recovery.
 *
 * @param mixed $link_identifier Database connection
 * @param int $transaction_id The limbo transaction ID
 * @return \Firebird\Transaction|false Transaction handle or false on error
 */
function fbird_reconnect_transaction(mixed $link_identifier, int $transaction_id): \Firebird\Transaction|false {}

// ============================================================================
// BATCH API FUNCTIONS (Firebird 4.0+ Bulk Operations)
// ============================================================================

/**
 * Create a batch from a prepared statement for bulk operations.
 *
 * @param mixed $query Prepared statement resource
 * @param mixed $trans_identifier Transaction resource (optional)
 * @return \Firebird\BatchHandle|false Batch handle or false on error
 */
function fbird_batch_create(mixed $query, mixed $trans_identifier = null): \Firebird\BatchHandle|false {}

/**
 * Add a row of parameters to the batch.
 *
 * @param mixed $batch Batch resource
 * @param mixed ...$args Parameter values
 * @return bool True on success, false on error
 */
function fbird_batch_add(mixed $batch, mixed ...$args): bool {}

/**
 * Create an inline BLOB in the batch context.
 * Returns a BLOB ID string in "HHHHHHHH:LLLL" format (13 characters).
 *
 * @param mixed $batch Batch resource from fbird_batch_create()
 * @param string $data BLOB content data
 * @param int $type BLOB subtype (0 = BINARY, 1 = TEXT, default 0)
 * @return string|false BLOB ID string or false on error
 */
function fbird_batch_add_blob(mixed $batch, string $data, int $type = 0): string|false {}

/**
 * Register an existing BLOB for use in a batch operation.
 *
 * @param mixed $batch Batch resource from fbird_batch_create()
 * @param string $blob_id Existing BLOB ID string from fbird_blob_close()
 * @return string|false Batch BLOB ID string or false on error
 */
function fbird_batch_register_blob(mixed $batch, string $blob_id): string|false {}

/**
 * Execute the batch and return results.
 *
 * @param mixed $batch Batch resource
 * @return array{total_processed: int, success_count: int, error_count: int, errors?: array<int, array{position: int, sqlstate: string, message: string}>}|false Results or false on error
 */
function fbird_batch_execute(mixed $batch): array|false {}

/**
 * Cancel the batch without executing.
 *
 * @param mixed $batch Batch resource
 * @return bool True on success, false on error
 */
function fbird_batch_cancel(mixed $batch): bool {}

/**
 * Returns the BLOB alignment requirement for this batch, in bytes.
 *
 * @param mixed $batch Batch resource from fbird_batch_create()
 * @return int|false Alignment in bytes (power of 2), or false on error
 * @since 7.0.0
 */
function fbird_batch_get_blob_alignment(mixed $batch): int|false {}

/**
 * @param mixed $batch
 * @param string $data
 * @return bool
 */
function fbird_batch_append_blob_data(mixed $batch, string $data): bool {}

/**
 * @param mixed $batch
 * @param string $data
 * @return bool
 */
function fbird_batch_add_blob_stream(mixed $batch, string $data): bool {}

/**
 * @param mixed $batch
 * @param string $bpb
 * @return bool
 */
function fbird_batch_set_default_bpb(mixed $batch, string $bpb): bool {}

// ============================================================================
// INSPECTION FUNCTIONS (Database/Attachment Management)
// ============================================================================

/**
 * Kill a database attachment by ID.
 *
 * @param mixed $link_or_trans Connection or transaction resource
 * @param int $attachment_id Attachment ID to kill
 * @return bool True on success, false on error
 */
function fbird_kill_attachment(mixed $link_or_trans, int $attachment_id): bool {}

/**
 * List attachments blocking a table.
 *
 * @param mixed $link_or_trans Connection or transaction resource
 * @param string $table_name Table name to check
 * @return array<int, array{attachment_id: int, user: string}>|false Blocker info or false on error
 */
function fbird_list_table_blockers(mixed $link_or_trans, string $table_name): array|false {}

/**
 * Force drop a table by killing blocking attachments.
 *
 * @param mixed $link_or_trans Connection or transaction resource
 * @param string $table_name Table name to drop
 * @return bool True on success, false on error
 */
function fbird_drop_table_force(mixed $link_or_trans, string $table_name): bool {}

/**
 * Set statement execution timeout (Firebird 4.0+).
 *
 * @param mixed $link_identifier Connection resource or Firebird\Connection
 * @param int   $milliseconds    Timeout in milliseconds (0 = no timeout)
 * @return bool True on success
 * @since 13.0.0
 */
function fbird_set_statement_timeout(mixed $link_identifier, int $milliseconds): bool {}

/**
 * Get statement execution timeout (Firebird 4.0+).
 *
 * @param mixed $link_identifier Connection resource or Firebird\Connection
 * @return int Timeout in milliseconds (0 = no timeout)
 * @since 13.0.0
 */
function fbird_get_statement_timeout(mixed $link_identifier): int {}

/**
 * Set connection idle timeout (Firebird 4.0+).
 *
 * @param mixed $link_identifier Connection resource or Firebird\Connection
 * @param int   $seconds         Timeout in seconds (0 = no timeout)
 * @return bool True on success
 * @since 13.0.0
 */
function fbird_set_idle_timeout(mixed $link_identifier, int $seconds): bool {}

/**
 * Get connection idle timeout (Firebird 4.0+).
 *
 * @param mixed $link_identifier Connection resource or Firebird\Connection
 * @return int Timeout in seconds (0 = no timeout)
 * @since 13.0.0
 */
function fbird_get_idle_timeout(mixed $link_identifier): int {}

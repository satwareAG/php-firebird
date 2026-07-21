<?php

/**
 * PHP Firebird Extension Stubs
 *
 * This file provides stub declarations for static analysis tools (PHPStan, Psalm)
 * and IDEs (PhpStorm, VSCode Intelephense) for code completion and type checking.
 *
 * The actual functions are provided by the php-firebird C extension.
 * This stub file does not contain any implementation.
 *
 * @package   php-firebird-stubs
 * @version   13.0.3
 * @author    satware AG <info@satware.com>
 * @copyright 2025-2026 satware AG
 * @license   PHP-3.01 https://www.php.net/license/3_01.txt
 * @link      https://github.com/satwareAG/php-firebird
 *
 * @since 7.0.0
 */

declare(strict_types=1);

// Skip if the extension is already loaded (runtime)
if (extension_loaded('fbird')) {
    return;
}

// ============================================================================
// GENERAL CONSTANTS
// ============================================================================

/** @var int Default/no special flags */
const FBIRD_DEFAULT = 0;

/** @var int Create database flag (used in fbird_query) */
const FBIRD_CREATE = 0;

/** @var int Force new connection (bypass connection reuse) */
const FBIRD_CONNECT_FORCE_NEW = 2;

/** @var int Extension version (100 for 10.x+ series) */
const FBIRD_VER = 100;

// ============================================================================
// EXCEPTION MODE CONSTANTS
// ============================================================================

/** @var int Exception mode: suppress errors (default) */
const FBIRD_EXCEPTION_MODE_SILENT = 0;

/** @var int Exception mode: throw exceptions on errors */
const FBIRD_EXCEPTION_MODE_THROW = 1;

/** @var int Exception mode: backward-compatible alias for SILENT (default) */
const FBIRD_EXCEPTION_MODE_COMPAT = 0;

// ============================================================================
// FETCH FLAGS
// ============================================================================

/** @var int Fetch BLOBs as strings instead of blob IDs */
const FBIRD_TEXT = 1;

/** @var int Fetch BLOBs as strings (alias for FBIRD_TEXT) */
const FBIRD_FETCH_BLOBS = 1;

/** @var int Fetch arrays as PHP arrays */
const FBIRD_FETCH_ARRAYS = 2;

/** @var int Return timestamps as Unix timestamps */
const FBIRD_UNIXTIME = 4;

/** @var int Return date/time columns as DateTimeImmutable objects */
const FBIRD_FETCH_DATE_OBJ = 8;

// ============================================================================
// TRANSACTION ACCESS MODES
// ============================================================================

/** @var int Read-write transaction (default) */
const FBIRD_WRITE = 1;

/** @var int Read-only transaction */
const FBIRD_READ = 2;

// ============================================================================
// TRANSACTION ISOLATION LEVELS
// ============================================================================

/** @var int SNAPSHOT isolation (CONCURRENCY) */
const FBIRD_CONCURRENCY = 4;

/** @var int READ COMMITTED isolation */
const FBIRD_COMMITTED = 8;

/** @var int SNAPSHOT TABLE STABILITY isolation (CONSISTENCY) */
const FBIRD_CONSISTENCY = 16;

/** @var int Read committed: use record version (default for COMMITTED) */
const FBIRD_REC_VERSION = 64;

/** @var int Read committed: no record version */
const FBIRD_REC_NO_VERSION = 32;

/** @var int Read committed with READ CONSISTENCY (Firebird 4.0+) */
const FBIRD_READ_CONSISTENCY = 32768;

// ============================================================================
// TRANSACTION LOCK RESOLUTION
// ============================================================================

/** @var int Wait for lock resolution */
const FBIRD_WAIT = 128;

/** @var int No wait - fail immediately on lock conflict */
const FBIRD_NOWAIT = 256;

/** @var int Lock timeout flag (used with FBIRD_WAIT) */
const FBIRD_LOCK_TIMEOUT = 512;

// ============================================================================
// TABLE RESERVATION LOCK TYPES
// ============================================================================

/** @var int Shared table lock */
const FBIRD_LOCK_SHARED = 1024;

/** @var int Protected table lock */
const FBIRD_LOCK_PROTECTED = 2048;

/** @var int Exclusive table lock */
const FBIRD_LOCK_EXCLUSIVE = 4096;

// ============================================================================
// TABLE RESERVATION ACCESS TYPES
// ============================================================================

/** @var int Read access for table reservation */
const FBIRD_LOCK_READ = 8192;

/** @var int Write access for table reservation */
const FBIRD_LOCK_WRITE = 16384;

// ============================================================================
// EVENT CONSTANTS
// ============================================================================

/** @var int Event timeout return value */
const FBIRD_EVENT_TIMEOUT = -2;

// ============================================================================
// BLOB SEEK CONSTANTS
// ============================================================================

/** @var int Seek from beginning of BLOB */
const FBIRD_BLOB_SEEK_SET = 0;

/** @var int Seek from current position */
const FBIRD_BLOB_SEEK_CUR = 1;

/** @var int Seek from end of BLOB */
const FBIRD_BLOB_SEEK_END = 2;

// ============================================================================
// SERVICE API CONSTANTS - BACKUP OPTIONS
// ============================================================================

/** @var int */
const FBIRD_BKP_IGNORE_CHECKSUMS = 1;
/** @var int */
const FBIRD_BKP_IGNORE_LIMBO = 2;
/** @var int */
const FBIRD_BKP_METADATA_ONLY = 4;
/** @var int */
const FBIRD_BKP_NO_GARBAGE_COLLECT = 8;
/** @var int */
const FBIRD_BKP_OLD_DESCRIPTIONS = 16;
/** @var int */
const FBIRD_BKP_NON_TRANSPORTABLE = 32;
/** @var int */
const FBIRD_BKP_CONVERT = 64;

// ============================================================================
// SERVICE API CONSTANTS - RESTORE OPTIONS
// ============================================================================

/** @var int */
const FBIRD_RES_DEACTIVATE_IDX = 256;
/** @var int */
const FBIRD_RES_NO_SHADOW = 512;
/** @var int */
const FBIRD_RES_NO_VALIDITY = 1024;
/** @var int */
const FBIRD_RES_ONE_AT_A_TIME = 2048;
/** @var int */
const FBIRD_RES_REPLACE = 4096;
/** @var int */
const FBIRD_RES_CREATE = 8192;
/** @var int */
const FBIRD_RES_USE_ALL_SPACE = 16384;

// ============================================================================
// SERVICE API CONSTANTS - DATABASE PROPERTIES
// ============================================================================

/** @var int */
const FBIRD_PRP_PAGE_BUFFERS = 5;
/** @var int */
const FBIRD_PRP_SWEEP_INTERVAL = 6;
/** @var int */
const FBIRD_PRP_SHUTDOWN_DB = 7;
/** @var int */
const FBIRD_PRP_DENY_NEW_TRANSACTIONS = 10;
/** @var int */
const FBIRD_PRP_DENY_NEW_ATTACHMENTS = 9;
/** @var int */
const FBIRD_PRP_RESERVE_SPACE = 11;
/** @var int */
const FBIRD_PRP_RES_USE_FULL = 35;
/** @var int */
const FBIRD_PRP_RES = 36;
/** @var int */
const FBIRD_PRP_WRITE_MODE = 12;
/** @var int */
const FBIRD_PRP_WM_ASYNC = 37;
/** @var int */
const FBIRD_PRP_WM_SYNC = 38;
/** @var int */
const FBIRD_PRP_ACCESS_MODE = 13;
/** @var int */
const FBIRD_PRP_AM_READONLY = 39;
/** @var int */
const FBIRD_PRP_AM_READWRITE = 40;
/** @var int */
const FBIRD_PRP_SET_SQL_DIALECT = 14;
/** @var int */
const FBIRD_PRP_ACTIVATE = 256;
/** @var int */
const FBIRD_PRP_DB_ONLINE = 512;

// ============================================================================
// SERVICE API CONSTANTS - REPAIR OPTIONS
// ============================================================================

/** @var int */
const FBIRD_RPR_CHECK_DB = 16;
/** @var int */
const FBIRD_RPR_IGNORE_CHECKSUM = 32;
/** @var int */
const FBIRD_RPR_KILL_SHADOWS = 64;
/** @var int */
const FBIRD_RPR_MEND_DB = 4;
/** @var int */
const FBIRD_RPR_VALIDATE_DB = 1;
/** @var int */
const FBIRD_RPR_FULL = 128;
/** @var int */
const FBIRD_RPR_SWEEP_DB = 2;

// ============================================================================
// SERVICE API CONSTANTS - STATISTICS
// ============================================================================

/** @var int */
const FBIRD_STS_DATA_PAGES = 1;
/** @var int */
const FBIRD_STS_DB_LOG = 2;
/** @var int */
const FBIRD_STS_HDR_PAGES = 4;
/** @var int */
const FBIRD_STS_IDX_PAGES = 8;
/** @var int */
const FBIRD_STS_SYS_RELATIONS = 16;

// ============================================================================
// SERVICE API CONSTANTS - SERVER INFO
// ============================================================================

/** @var int */
const FBIRD_SVC_SERVER_VERSION = 55;
/** @var int */
const FBIRD_SVC_IMPLEMENTATION = 56;
/** @var int */
const FBIRD_SVC_GET_ENV = 59;
/** @var int */
const FBIRD_SVC_GET_ENV_LOCK = 60;
/** @var int */
const FBIRD_SVC_GET_ENV_MSG = 61;
/** @var int */
const FBIRD_SVC_USER_DBPATH = 58;
/** @var int */
const FBIRD_SVC_SVR_DB_INFO = 50;
/** @var int */
const FBIRD_SVC_GET_USERS = 68;

// ============================================================================
// CONNECTION FUNCTIONS
// ============================================================================

/**
 * Connect to a Firebird database.
 *
 * @param string      $database Database path or connection string
 * @param string|null $username Username for authentication
 * @param string|null $password Password for authentication
 * @param string|null $charset  Character set for the connection
 * @param int         $buffers  Number of database cache buffers
 * @param int         $dialect  SQL dialect (1, 2, or 3)
 * @param string|null $role     SQL role name
 * @param int         $flags    Connection flags (e.g. FBIRD_CONNECT_FORCE_NEW)
 * @return \Firebird\Connection|false Connection object on success, false on failure
 * @since 7.0.0
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
): Firebird\Connection|false {}

/**
 * Open a persistent connection to a Firebird database.
 *
 * @param string      $database Database path or connection string
 * @param string|null $username Username for authentication
 * @param string|null $password Password for authentication
 * @param string|null $charset  Character set for the connection
 * @param int         $buffers  Number of database cache buffers
 * @param int         $dialect  SQL dialect (1, 2, or 3)
 * @param string|null $role     SQL role name
 * @return Firebird\Connection|false Connection object on success, false on failure
 * @since 7.0.0
 */
function fbird_pconnect(
    string $database,
    ?string $username = null,
    ?string $password = null,
    ?string $charset = null,
    int $buffers = 0,
    int $dialect = 3,
    ?string $role = null
): Firebird\Connection|false {}

/**
 * Close a Firebird database connection.
 *
 * @param Firebird\Connection|null $connection Connection object
 * @return bool True on success, false on failure
 * @since 7.0.0
 */
function fbird_close(mixed $connection = null): bool {}

/**
 * Ping a server connection to check if it is still alive.
 * @param resource|null $connection Connection handle (defaults to last opened)
 * @return bool True if connection is alive, false otherwise.
 */
function fbird_ping(?object $connection = null): bool {}

/**
 * Get the Firebird server version string from a connection.
 * @param resource|null $connection Connection handle (defaults to last opened)
 * @return string|false Version string (e.g. "LI-V4.0.7.3271 Firebird 4.0 ..."), or false on error.
 */
function fbird_server_version(?object $connection = null): string|false {}

/**
 * Drop a Firebird database.
 *
 * @param mixed $connection Connection resource or database path (v9.0.0+)
 * @param string|null          $username   Username (if path used)
 * @param string|null          $password   Password (if path used)
 * @return bool True on success, false on failure
 * @since 7.0.0
 */
function fbird_drop_db(mixed $connection = null, ?string $username = null, ?string $password = null): bool {}

/**
 * Create a new Firebird database.
 *
 * Creates a database at the specified path with optional credentials, charset,
 * and page size. Returns a connection object to the newly created database.
 *
 * @param string      $database  Database connection string (e.g. "host:/path/to/db.fdb")
 * @param string|null $username  Username (default: SYSDBA)
 * @param string|null $password  Password
 * @param string|null $charset   Character set (default: UTF8)
 * @param int|null    $page_size Page size in bytes (default: 8192)
 * @return Firebird\Connection|false Connection object on success, false on failure
 * @since 9.0.0
 */
function fbird_create_database(
    string $database,
    ?string $username = null,
    ?string $password = null,
    ?string $charset = null,
    ?int $page_size = null
): Firebird\Connection|false {}

/**
 * Get the last insert ID using a named generator/sequence.
 *
 * Firebird does not have implicit auto-increment IDs. This function queries
 * the current value of a named generator (sequence) using GEN_ID(sequence, 0).
 *
 * @param mixed $link_identifier Connection object (required, pass null for default connection)
 * @param string|null $sequence   Generator/sequence name (optional but required at runtime)
 * @return int|false Current generator value or false on failure
 * @since 8.2.0
 */
function fbird_last_insert_id(mixed $link_identifier, ?string $sequence = null): int|false {}

// ============================================================================
// QUERY FUNCTIONS
// ============================================================================

/**
 * Execute a query against a Firebird database.
 *
 * @param mixed $link_or_query Connection/transaction or query string
 * @param mixed           ...$args       Additional arguments
 * @return \Firebird\ResultSet|int|bool ResultSet object for SELECT, affected-row count for DML, false on error
 * @since 7.0.0
 */
function fbird_query(mixed $link_or_query, mixed ...$args): \Firebird\ResultSet|int|bool {}

/**
 * Prepare a SQL statement for later execution.
 *
 * @param mixed $link_or_trans_or_query   First argument
 * @param mixed $link_or_trans_or_query_2 Second argument
 * @param string|null          $query                    Query string
 * @return \Firebird\Statement|false Statement object (v11.1.0+, was resource before)
 * @since 7.0.0
 */
function fbird_prepare(
    mixed $link_or_trans_or_query,
    mixed $link_or_trans_or_query_2 = null,
    ?string $query = null
): \Firebird\Statement|false {}

/**
 * Prepare a SQL statement with a fixed, non-shifting signature.
 *
 * Unlike fbird_prepare() which uses argument-shifting heuristics,
 * this function has a clear signature: connection first, query second,
 * optional transaction third.
 *
 * @param mixed    $link_identifier Connection resource
 * @param string      $query           SQL query string
 * @param mixed $trans_handle  Transaction resource (optional)
 * @return \Firebird\Statement|false Statement object (v11.1.0+, was resource before)
 * @since 9.0.0
 */
function fbird_prepare_ex(
    mixed $link_identifier,
    string $query,
    mixed $trans_handle = null
): \Firebird\Statement|false {}

/**
 * Execute a prepared statement.
 *
 * @param resource|\Firebird\ResultSet $query Prepared statement resource or ResultSet object
 * @param mixed    ...$bind_args Bind parameters
 * @return \Firebird\ResultSet|int|bool ResultSet object for SELECT, affected-row count for DML, false on error
 * @since 7.0.0
 */
function fbird_execute(mixed $query, mixed ...$bind_args): \Firebird\ResultSet|int|bool {}

/**
 * Execute a DML/DDL statement with parameters within an explicit transaction.
 *
 * Prepares and executes a non-SELECT SQL statement atomically. Throws an error
 * if a SELECT statement is given; use fbird_execute_query() for those.
 *
 * @param mixed               $trans_handle Transaction resource
 * @param string                 $query        SQL DML/DDL statement
 * @param array<int, mixed>|null $params       Bind parameters (optional)
 * @return int|false Affected-row count (0 for DDL) or false on error
 * @since 7.0.0
 */
function fbird_execute_statement(mixed $trans_handle, string $query, ?array $params = null): int|false {}

/**
 * Execute a SELECT/RETURNING statement with parameters within an explicit transaction.
 *
 * Prepares and executes a SELECT query atomically and returns a result object.
 * Throws an error if a DML statement is given; use fbird_execute_statement() for those.
 *
 * @param mixed               $trans_handle Transaction resource
 * @param string                 $query        SQL SELECT or RETURNING statement
 * @param array<int, mixed>|null $params       Bind parameters (optional)
 * @return \Firebird\ResultSet|false Result object or false on error
 * @since 7.0.0
 */
function fbird_execute_query(mixed $trans_handle, string $query, ?array $params = null): \Firebird\ResultSet|false {}

/**
 * Execute any SQL statement via an auto-managed transaction on an OO API connection.
 *
 * Requires a Firebird 3.0+ OO API connection. Auto-detects
 * SELECT vs DML/DDL and returns the appropriate result.
 *
 * @param mixed               $link_identifier Database connection resource (OO API)
 * @param string                 $query           SQL statement
 * @param array<int, mixed>|null $params          Bind parameters (optional)
 * @return \Firebird\ResultSet|int|false ResultSet object (SELECT), int (DML affected rows), or false
 * @since 7.0.0
 */
function fbird_execute_auto(mixed $link_identifier, string $query, ?array $params = null): \Firebird\ResultSet|int|false {}

/**
 * Execute a parameterized query with explicit link and transaction.
 *
 * Required by doctrine-firebird-driver: passes both connection and explicit
 * transaction handle simultaneously (unlike fbird_execute_query which infers
 * the link from the transaction).
 *
 * @param mixed     $link_identifier Connection resource
 * @param mixed     $trans_handle    Transaction resource
 * @param string       $query           SQL statement
 * @param array<mixed>|null $params     Bind parameters (optional)
 * @return \Firebird\ResultSet|int|false           ResultSet for SELECT, row count for DML, false on error
 * @since 7.1.0
 */
function fbird_query_params_tx(mixed $link_identifier, mixed $trans_handle, string $query, ?array $params = null): \Firebird\ResultSet|int|false {}

/**
 * Free a prepared statement.
 *
 * @param mixed $query Prepared statement resource
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_free_query(mixed $query): bool {}

/**
 * Free a result set.
 *
 * @param mixed $result Result resource
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_free_result(mixed $result): bool {}

// ============================================================================
// FETCH FUNCTIONS
// ============================================================================

/**
 * Fetch a row as a numeric array.
 *
 * @param mixed $result      Result resource
 * @param int      $fetch_flags Fetch flags (e.g. FBIRD_TEXT, FBIRD_FETCH_DATE_OBJ)
 * @return array<int, mixed>|false Row data or false
 * @since 7.0.0
 */
function fbird_fetch_row(mixed $result, int $fetch_flags = 0): array|false {}

/**
 * Fetch a row as an associative array.
 *
 * @param mixed $result      Result resource
 * @param int      $fetch_flags Fetch flags (e.g. FBIRD_TEXT, FBIRD_FETCH_DATE_OBJ)
 * @return array<string, mixed>|false Row data or false
 * @since 7.0.0
 */
function fbird_fetch_assoc(mixed $result, int $fetch_flags = 0): array|false {}

/**
 * Fetch a row as an array with both numeric and associative indices.
 * @param resource $result Result handle from fbird_query()
 * @param int $fetch_flags Optional fetch flags (e.g. FBIRD_FETCH_BLOBS)
 * @return array|false Array with both numeric and associative keys, or false on no more rows.
 */
function fbird_fetch_array(mixed $result, int $fetch_flags = 0): array|false {}

/**
 * Fetch a row as an object.
 *
 * @param mixed $result      Result resource
 * @param int      $fetch_flags Fetch flags (e.g. FBIRD_TEXT, FBIRD_FETCH_DATE_OBJ)
 * @return object|false Row object or false
 * @since 7.0.0
 */
function fbird_fetch_object(mixed $result, int $fetch_flags = 0): object|false {}

/**
 * Name a result set for cursor operations.
 *
 * @param mixed $result Result resource
 * @param string   $name   Cursor name
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_name_result(mixed $result, string $name): bool {}

// ============================================================================
// FIELD/PARAMETER INFO FUNCTIONS
// ============================================================================

/**
 * Get number of fields in result set.
 *
 * @param mixed $result Result resource
 * @return int|false Number of fields or false
 * @since 7.0.0
 */
function fbird_num_fields(mixed $result): int|false {}

/**
 * Get number of parameters in prepared statement.
 *
 * @param mixed $query Prepared statement resource
 * @return int|false Number of parameters or false
 * @since 7.0.0
 */
function fbird_num_params(mixed $query): int|false {}

/**
 * Get number of affected rows.
 *
 * @param mixed $link Connection or statement resource
 * @return int Number of affected rows
 * @since 7.0.0
 */
function fbird_affected_rows(mixed $link = null): int {}

/**
 * Get field information.
 *
 * @param mixed $result       Result resource
 * @param int      $field_number Field index (0-based)
 * @return array<string, mixed>|false Field info or false
 * @since 7.0.0
 */
function fbird_field_info(mixed $result, int $field_number): array|false {}

/**
 * Get parameter information.
 *
 * @param mixed $query        Prepared statement resource
 * @param int      $param_number Parameter index (0-based)
 * @return array<string, mixed>|false Parameter info or false
 * @since 7.0.0
 */
function fbird_param_info(mixed $query, int $param_number): array|false {}

/**
 * List user tables in the database.
 * @param resource|null $connection Connection handle
 * @return array|false Array of table names, or false on error.
 */
function fbird_list_tables(?object $connection = null): array|false {}

/**
 * List field names for a table.
 * @param resource $connection Connection handle
 * @param string $table_name Table name
 * @return array|false Array of field names, or false on error.
 */
function fbird_list_fields(object $connection, string $table_name): array|false {}

/**
 * Get metadata for a table's columns (type, length, scale, nullable).
 * @param resource $connection Connection handle
 * @param string $table_name Table name
 * @return array|false Associative array keyed by field name, or false on error.
 */
function fbird_meta_data(object $connection, string $table_name): array|false {}

// ============================================================================
// TRANSACTION FUNCTIONS
// ============================================================================

/**
 * Start a transaction.
 *
 * @param mixed $link_or_flags Connection resource or transaction flags
 * @param mixed             ...$args       Additional connection resources for multi-db transaction
 * @return \Firebird\Transaction|false Transaction object
 * @since 7.0.0
 */
function fbird_trans(mixed $link_or_flags = null, mixed ...$args): \Firebird\Transaction|false {}

/**
 * Start a transaction with options.
 *
 * @param mixed                                     $link    Connection resource
 * @param array<string, array<string, int>|bool|int>|null $options Transaction options
 * @return \Firebird\Transaction|false Transaction object
 * @since 7.0.0
 */
function fbird_trans_start(mixed $link, ?array $options = null): \Firebird\Transaction|false {}

/**
 * Commit a transaction.
 *
 * @param mixed $link Transaction or connection resource
 * @return bool True on success, false on failure
 * @since 7.0.0
 */
function fbird_commit(mixed $link = null): bool {}

/**
 * Rollback a transaction.
 *
 * @param mixed $link Transaction or connection resource
 * @return bool True on success, false on failure
 * @since 7.0.0
 */
function fbird_rollback(mixed $link = null): bool {}

/**
 * Commit and retain a transaction.
 *
 * @param mixed $link Transaction or connection resource
 * @return bool True on success, false on failure
 * @since 7.0.0
 */
function fbird_commit_ret(mixed $link = null): bool {}

/**
 * Rollback and retain a transaction.
 *
 * @param mixed $link Transaction or connection resource
 * @return bool True on success, false on failure
 * @since 7.0.0
 */
function fbird_rollback_ret(mixed $link = null): bool {}

/**
 * Create a savepoint.
 *
 * @param mixed $link Transaction resource
 * @param string   $name Savepoint name
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_savepoint(mixed $link, string $name): bool {}

/**
 * Rollback to a savepoint.
 *
 * @param mixed $link Transaction resource
 * @param string   $name Savepoint name
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_rollback_savepoint(mixed $link, string $name): bool {}

/**
 * Release a savepoint.
 *
 * @param mixed $link Transaction resource
 * @param string   $name Savepoint name
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_release_savepoint(mixed $link, string $name): bool {}

/**
 * Get transaction information.
 *
 * @param mixed $trans_handle Transaction resource
 * @return array<string, mixed>|false Transaction info or false
 * @since 7.0.0
 */
function fbird_trans_info(mixed $trans_handle): array|false {}

// ============================================================================
// BLOB FUNCTIONS
// ============================================================================

/**
 * Create a blob for adding data.
 *
 * @param resource|\Firebird\Connection|null $link Database connection
 * @return \Firebird\Blob|false Blob handle or false
 * @since 7.0.0
 */
function fbird_blob_create(mixed $link = null): \Firebird\Blob|false {}

/**
 * Add data to a blob.
 *
 * @param mixed $blob Blob handle
 * @param string   $data Data to add
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_blob_add(mixed $blob, string $data): bool {}

/**
 * Close a blob and get its ID.
 *
 * @param mixed $blob Blob handle
 * @return string|false Blob ID or false
 * @since 7.0.0
 */
function fbird_blob_close(mixed $blob): string|false {}

/**
 * Cancel a blob.
 *
 * @param mixed $blob Blob handle
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_blob_cancel(mixed $blob): bool {}

/**
 * Open a blob for reading.
 *
 * @param resource|\Firebird\Connection|string $link_or_blob_id Connection or blob ID
 * @param string|null                          $blob_id         Blob ID
 * @return \Firebird\Blob|false Blob handle or false
 * @since 7.0.0
 */
function fbird_blob_open(mixed $link_or_blob_id, ?string $blob_id = null): \Firebird\Blob|false {}

/**
 * Get data from a blob.
 *
 * @param mixed $blob   Blob handle
 * @param int      $length Number of bytes to read
 * @return string|false Data or false
 * @since 7.0.0
 */
function fbird_blob_get(mixed $blob, int $length): string|false {}

/**
 * Output a blob to the browser.
 *
 * @param mixed $link_or_id Connection or blob ID
 * @param string|null     $blob_id    Blob ID
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_blob_echo(mixed $link_or_id, ?string $blob_id = null): bool {}

/**
 * Get blob information.
 *
 * @param mixed $link_or_id Connection or blob ID
 * @param string|null     $blob_id    Blob ID
 * @return array<string, mixed>|false Blob info or false
 * @since 7.0.0
 */
function fbird_blob_info(mixed $link_or_id, ?string $blob_id = null): array|false {}

/**
 * Import a file as a blob.
 *
 * @param mixed $link Database connection
 * @param mixed $file File handle (e.g. from fopen)
 * @return string|false Blob ID or false
 * @since 7.0.0
 */
function fbird_blob_import(mixed $link, mixed $file): string|false {}

/**
 * Create a blob stream.
 *
 * @param mixed $link Database connection
 * @return resource|false Stream handle or false
 * @since 7.0.0
 */
function fbird_blob_create_stream(mixed $link = null): mixed {}

/**
 * Open a blob as a stream.
 *
 * @param mixed $link_or_id Connection or blob ID
 * @param string|null     $blob_id    Blob ID
 * @return resource|false Stream handle or false
 * @since 7.0.0
 */
function fbird_blob_open_stream(mixed $link_or_id, ?string $blob_id = null): mixed {}

/**
 * Create a seekable blob.
 *
 * @param resource|\Firebird\Connection|null $link Database connection
 * @return \Firebird\Blob|false Blob handle or false
 * @since 7.0.0
 */
function fbird_blob_create_seekable(mixed $link = null): \Firebird\Blob|false {}

/**
 * Open a blob as seekable.
 *
 * @param resource|\Firebird\Connection|string $link_or_id Connection or blob ID
 * @param string|null                          $blob_id    Blob ID
 * @return \Firebird\Blob|false Blob handle or false
 * @since 7.0.0
 */
function fbird_blob_open_seekable(mixed $link_or_id, ?string $blob_id = null): \Firebird\Blob|false {}

/**
 * Seek within a blob.
 *
 * @param mixed $blob   Blob handle
 * @param int      $offset Offset in bytes
 * @param int      $whence Seek mode (FBIRD_BLOB_SEEK_*)
 * @return int|false New position or false
 * @since 7.0.0
 */
function fbird_blob_seek(mixed $blob, int $offset, int $whence = 0): int|false {}

// ============================================================================
// GENERATOR FUNCTIONS
// ============================================================================

/**
 * Get the next value from a generator/sequence.
 *
 * @param string        $generator Generator name
 * @param int           $increment Increment value
 * @param mixed $link      Connection resource
 * @return int|string|false Generator value or false
 * @since 7.0.0
 */
function fbird_gen_id(string $generator, int $increment = 1, mixed $link = null): int|string|false {}

// ============================================================================
// ERROR FUNCTIONS
// ============================================================================

/**
 * Return the last error message.
 *
 * @return string|false Error message or false
 * @since 7.0.0
 */
function fbird_errmsg(?object $link_identifier = null): string|false {}

/**
 * Return the last error code.
 *
 * @return int|false Error code or false
 * @since 7.0.0
 */
function fbird_errcode(?object $link_identifier = null): int|false {}

/**
 * Return the SQLSTATE error code.
 *
 * @return string|false 5-character SQLSTATE code or false
 * @since 7.0.0
 */
function fbird_sqlstate(?object $link_identifier = null): string|false {}

/**
 * Escape a string for use in SQL.
 *
 * @param string $string String to escape
 * @return string Escaped string
 * @since 7.0.0
 */
function fbird_escape_string(string $string): string {}

/**
 * Set the exception mode.
 *
 * @param int $mode FBIRD_EXCEPTION_MODE_SILENT or FBIRD_EXCEPTION_MODE_THROW
 * @return bool True on success
 * @since 9.0.0
 */
function fbird_set_exception_mode(int $mode): bool {}

/**
 * Get the current exception mode.
 *
 * @return int Current exception mode (FBIRD_EXCEPTION_MODE_SILENT or FBIRD_EXCEPTION_MODE_THROW)
 * @since 9.0.0
 */
function fbird_get_exception_mode(): int {}

// ============================================================================
// EVENT FUNCTIONS
// ============================================================================

/**
 * Wait for a database event (blocking).
 *
 * @param mixed $link_or_event Connection or event name
 * @param string          ...$events     Event names
 * @return string|false Event name or false
 * @since 7.0.0
 */
function fbird_wait_event(mixed $link_or_event, string ...$events): string|false {}

/**
 * Set an event handler callback.
 *
 * @param resource|callable $link_or_callback Connection or callback
 * @param callable|string   $callback_or_event Callback or event name
 * @param string            ...$events        Event names
 * @return \Firebird\Event|false Event handler or false
 * @since 7.0.0
 */
function fbird_set_event_handler(mixed $link_or_callback, mixed $callback_or_event, string ...$events): \Firebird\Event|false {}

/**
 * Poll for events (non-blocking).
 *
 * @param mixed $event      Event handler resource
 * @param int      $timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return array<string, int>|int|false Event counts or timeout (-2)
 * @since 7.0.0
 */
function fbird_poll_event(mixed $event, int $timeout_ms = 0): array|int|false {}

/**
 * Free an event handler.
 *
 * @param mixed $event Event handler resource
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_free_event_handler(mixed $event): bool {}

// ============================================================================
// SERVICE MANAGER FUNCTIONS
// ============================================================================

/**
 * Attach to the service manager.
 *
 * @param string $host     Host name (e.g. "localhost")
 * @param string $username Username
 * @param string $password Password
 * @return Firebird\Service|false Service handle or false
 * @since 7.0.0
 */
function fbird_service_attach(string $host, string $username, string $password): Firebird\Service|false {}

/**
 * Detach from the service manager.
 *
 * @param Firebird\Service $service Service handle
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_service_detach(mixed $service): bool {}

/**
 * Backup a database.
 *
 * @param Firebird\Service $service Service handle
 * @param string   $source  Source database path
 * @param string   $dest    Destination backup file path
 * @param int      $options Backup options (FBIRD_BKP_*)
 * @param bool     $verbose Verbose output
 * @return mixed Result
 * @since 7.0.0
 */
function fbird_backup(mixed $service, string $source, string $dest, int $options = 0, bool $verbose = false): mixed {}

/**
 * Restore a database.
 *
 * @param Firebird\Service $service Service handle
 * @param string   $source  Source backup file path
 * @param string   $dest    Destination database path
 * @param int      $options Restore options (FBIRD_RES_*)
 * @param bool     $verbose Verbose output
 * @return mixed Result
 * @since 7.0.0
 */
function fbird_restore(mixed $service, string $source, string $dest, int $options = 0, bool $verbose = false): mixed {}

/**
 * Perform database maintenance.
 *
 * @param Firebird\Service $service  Service handle
 * @param string   $db       Database path
 * @param int      $action   Maintenance action (FBIRD_PRP_* or FBIRD_RPR_*)
 * @param int      $argument Action argument
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_maintain_db(mixed $service, string $db, int $action, int $argument = 0): bool {}

/**
 * Get database information via service manager.
 *
 * @param Firebird\Service $service  Service handle
 * @param string   $db       Database path
 * @param int      $action   Information action
 * @param int      $argument Action argument
 * @return string|false Information string or false
 * @since 7.0.0
 */
function fbird_db_info(mixed $service, string $db, int $action, int $argument = 0): string|false {}

/**
 * Get server information via service manager.
 *
 * @param Firebird\Service $service Service handle
 * @param int      $action  Information action (FBIRD_SVC_*)
 * @return string|false Information string or false
 * @since 7.0.0
 */
function fbird_server_info(mixed $service, int $action): string|false {}

// ============================================================================
// USER MANAGEMENT FUNCTIONS
// ============================================================================

/**
 * Add a user to the Firebird security database.
 *
 * @param Firebird\Service $service     Service handle
 * @param string      $username    Username
 * @param string      $password    Password
 * @param string|null $first_name  First name
 * @param string|null $middle_name Middle name
 * @param string|null $last_name   Last name
 * @return bool True on success
 * @since 7.0.0
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
 * Modify a user in the Firebird security database.
 *
 * @param Firebird\Service $service     Service handle
 * @param string      $username    Username
 * @param string      $password    Password
 * @param string|null $first_name  First name
 * @param string|null $middle_name Middle name
 * @param string|null $last_name   Last name
 * @return bool True on success
 * @since 7.0.0
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
 * Delete a user from the Firebird security database.
 *
 * @param Firebird\Service $service  Service handle
 * @param string   $username Username
 * @return bool True on success
 * @since 7.0.0
 */
function fbird_delete_user(mixed $service, string $username): bool {}

// ============================================================================
// VERSION FUNCTIONS
// ============================================================================

/**
 * Get Firebird client library version string.
 *
 * @return float Version number
 * @since 7.0.0
 */
function fbird_get_client_version(): float {}

/**
 * Get Firebird client library major version.
 *
 * @return int Major version number
 * @since 7.0.0
 */
function fbird_get_client_major_version(): int {}

/**
 * Get Firebird client library minor version.
 *
 * @return int Minor version number
 * @since 7.0.0
 */
function fbird_get_client_minor_version(): int {}

// ============================================================================
// CONNECTION INFO FUNCTIONS
// ============================================================================

/**
 * Get database connection statistics and information.
 *
 * @param mixed $link_identifier Database connection resource
 * @return array<string, int>|false Connection statistics or false
 * @since 7.0.0
 */
function fbird_connection_info(mixed $link_identifier = null): array|false {}

// ============================================================================
// LIMBO TRANSACTION FUNCTIONS
// ============================================================================

/**
 * Get list of limbo (in-doubt) transaction IDs.
 *
 * @param mixed $link_identifier Database connection
 * @param int           $max_count       Maximum count
 * @return array<int, int>|false Array of transaction IDs or false
 * @since 7.0.0
 */
function fbird_get_limbo_transactions(mixed $link_identifier = null, int $max_count = 100): array|false {}

/**
 * Reconnect to a limbo transaction for recovery.
 *
 * @param mixed $link_identifier Database connection
 * @param int      $transaction_id  Transaction ID
 * @return \Firebird\Transaction|false Transaction handle or false
 * @since 7.0.0
 */
function fbird_reconnect_transaction(mixed $link_identifier, int $transaction_id): \Firebird\Transaction|false {}

// ============================================================================
// BATCH API FUNCTIONS (Firebird 4.0+)
// ============================================================================

/**
 * Create a batch from a prepared statement.
 *
 * @param mixed      $query            Prepared statement
 * @param mixed $trans_identifier Transaction handle
 * @return \Firebird\BatchHandle|false Batch handle or false
 * @since 9.0.0
 */
function fbird_batch_create(mixed $query, mixed $trans_identifier = null): \Firebird\BatchHandle|false {}

/**
 * Add a row of parameters to the batch.
 *
 * @param mixed $batch  Batch handle
 * @param mixed    ...$args Parameters matching the statement
 * @return bool True on success
 * @since 9.0.0
 */
function fbird_batch_add(mixed $batch, mixed ...$args): bool {}

/**
 * Create an inline BLOB in batch context.
 *
 * @param mixed $batch Batch handle
 * @param string   $data  BLOB data
 * @param int      $type  BLOB type
 * @return string|false BLOB ID or false
 * @since 9.0.0
 */
function fbird_batch_add_blob(mixed $batch, string $data, int $type = 0): string|false {}

/**
 * Register an existing BLOB for batch use.
 *
 * @param mixed $batch   Batch handle
 * @param string   $blob_id BLOB ID
 * @return string|false Batch BLOB ID or false
 * @since 9.0.0
 */
function fbird_batch_register_blob(mixed $batch, string $blob_id): string|false {}

/**
 * Execute the batch.
 *
 * @param mixed $batch Batch handle
 * @return array{total_processed: int, success_count: int, error_count: int}|false Results or false
 * @since 9.0.0
 */
function fbird_batch_execute(mixed $batch): array|false {}

/**
 * Cancel the batch without executing.
 *
 * @param mixed $batch Batch handle
 * @return bool True on success
 * @since 9.0.0
 */
function fbird_batch_cancel(mixed $batch): bool {}

/**
 * Returns the BLOB alignment requirement for this batch, in bytes.
 *
 * Only available with Firebird 4.0+.
 *
 * @param mixed $batch Batch resource
 * @return int|false Alignment in bytes, or false on error
 * @since 9.0.0
 */
function fbird_batch_get_blob_alignment(mixed $batch): int|false {}

/**
 * Append a chunk of data to the BLOB currently being constructed in the batch.
 *
 * @param mixed $batch Batch resource
 * @param string $data Binary chunk
 * @return bool TRUE on success, FALSE on failure
 * @since 9.0.0
 */
function fbird_batch_append_blob_data(mixed $batch, string $data): bool {}

/**
 * Add BLOB data to the batch using the IBatch addBlobStream protocol.
 *
 * @param mixed $batch Batch resource
 * @param string $data Binary BLOB stream data
 * @return bool TRUE on success, FALSE on failure
 * @since 9.0.0
 */
function fbird_batch_add_blob_stream(mixed $batch, string $data): bool {}

/**
 * Set the default BLOB Property Block (BPB) for all BLOBs in this batch.
 *
 * @param mixed $batch Batch resource
 * @param string $bpb Raw binary BPB data
 * @return bool TRUE on success, FALSE on failure
 * @since 9.0.0
 */
function fbird_batch_set_default_bpb(mixed $batch, string $bpb): bool {}

// ============================================================================
// STATEMENT/SESSION TIMEOUT FUNCTIONS (Firebird 4.0+)
// ============================================================================

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

/**
 * Set per-statement execution timeout (Firebird 4.0+).
 * Overrides the connection-level default for this specific statement.
 * @param resource $query Statement handle from fbird_prepare()
 * @param int $milliseconds Timeout in milliseconds (0 = no timeout)
 * @return bool True on success, false on error.
 */
function fbird_stmt_set_timeout(mixed $query, int $milliseconds): bool {}

/**
 * Get per-statement execution timeout (Firebird 4.0+).
 * @param resource $query Statement handle from fbird_prepare()
 * @return int Timeout in milliseconds (0 = no timeout or error).
 */
function fbird_stmt_get_timeout(mixed $query): int {}

// ============================================================================
// INSPECTION FUNCTIONS
// ============================================================================

/**
 * Kill a database attachment by ID.
 *
 * @param mixed $link_or_trans Connection or transaction
 * @param int      $attachment_id Attachment ID
 * @return bool True on success
 * @since 8.0.0
 */
function fbird_kill_attachment(mixed $link_or_trans, int $attachment_id): bool {}

/**
 * List attachments blocking a table.
 *
 * @param mixed $link_or_trans Connection or transaction
 * @param string   $table_name    Table name
 * @return array<int, array{attachment_id: int, user: string}>|false Blockers or false
 * @since 8.0.0
 */
function fbird_list_table_blockers(mixed $link_or_trans, string $table_name): array|false {}

/**
 * Force drop a table by killing blockers.
 *
 * @param mixed $link_or_trans Connection or transaction
 * @param string   $table_name    Table name
 * @return bool True on success
 * @since 8.0.0
 */
function fbird_drop_table_force(mixed $link_or_trans, string $table_name): bool {}

// ============================================================================
// EXTENSION-PROVIDED CLASSES (Firebird namespace)
// ============================================================================
// Note: Classes in the Firebird namespace are defined in a separate file
// to comply with PHP namespace rules.
// See: firebird-classes.php

// #362-#381: M2 procedural parity functions

/**
 * Seek to a specific row number in a result set.
 */
function fbird_data_seek(mixed $result, int $row_number): bool {}

/**
 * Fetch all rows as an array of associative arrays.
 */
function fbird_fetch_all(mixed $result, int $fetch_flags = 0): array|false {}

/**
 * Fetch the next row and return a single column value.
 */
function fbird_fetch_column(mixed $result, int $column = 0): mixed {}

/**
 * Reset a prepared statement for re-execution (close cursor, keep statement).
 */
function fbird_stmt_reset(mixed $query): bool {}

/**
 * Escape a string for use as a SQL literal (wraps in single quotes, doubles internal quotes).
 */
function fbird_escape_literal(string $string): string {}

/**
 * Escape a string for use as a SQL identifier (wraps in double quotes, doubles internal quotes).
 */
function fbird_escape_identifier(string $identifier): string {}

/**
 * Return metadata for all output columns of a prepared statement.
 */
function fbird_result_metadata(mixed $query): array|false {}

/**
 * Export a blob to a file.
 */
function fbird_blob_export(mixed $connection, mixed $blob_id, string $filename): bool {}

/**
 * Execute multiple SQL statements separated by semicolons.
 */
function fbird_multi_query(mixed $link_identifier, string $query): mixed {}

/**
 * Get a statement attribute.
 */
function fbird_stmt_attr_get(mixed $query, int $attribute): int|string|false {}

/**
 * Set a statement attribute.
 */
function fbird_stmt_attr_set(mixed $query, int $attribute, mixed $value): bool {}

// M2 Procedural Parity: remaining functions (#366-#380, #476-#478)

function fbird_set_charset(mixed $connection, string $charset): bool {}
function fbird_character_set_name(mixed $connection = null): string {}
function fbird_bind_param(mixed $statement, array $params): bool {}
function fbird_bind_result(mixed $statement, string $column, mixed &$var): bool {}
function fbird_debug(?string $message = null): bool {}
function fbird_blob_truncate(mixed $blob_handle): bool {}
function fbird_blob_erase(mixed $blob_handle): bool {}
function fbird_blob_flush(mixed $blob_handle): bool {}
function fbird_send_long_data(mixed $statement, int $param_num, mixed $data): bool {}
function fbird_nbak(mixed $service_handle, int $level, string $database): bool {}
function fbird_trace_start(mixed $service_handle, string $config): bool {}
function fbird_trace_stop(mixed $service_handle, int $trace_id): bool {}

function fbird_set_session_timezone(mixed $connection, string $timezone): bool {}
function fbird_get_session_timezone(mixed $connection = null): string|false {}
function fbird_dump_debug_info(mixed $connection = null): bool {}

function fbird_error_list(?object $link_identifier = null): array|false {}
function fbird_error_field(?object $link_identifier = null): array|false {}

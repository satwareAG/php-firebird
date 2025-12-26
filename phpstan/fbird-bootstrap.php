<?php

/**
 * PHPStan bootstrap file for php-firebird extension constants.
 *
 * This file defines the constants at runtime for static analysis.
 * Used via bootstrapFiles in phpstan.neon.
 *
 * @package   Firebird
 * @author    Michael Wegener <mw@satware.com>
 * @copyright satware AG 2025
 * @license   PHP License (same as PHP itself)
 */

// Only define if not already defined by the extension
if (!defined('FBIRD_DEFAULT')) {
    // ========================================================================
    // GENERAL CONSTANTS
    // ========================================================================

    /** Default/no special flags */
    define('FBIRD_DEFAULT', 0);

    /** Create database flag */
    define('FBIRD_CREATE', 0);

    /** Force new connection (bypass connection reuse) */
    define('FBIRD_CONNECT_FORCE_NEW', 2);

    /** Extension version */
    define('FBIRD_VER', 10);

    // ========================================================================
    // FETCH FLAGS
    // ========================================================================

    /** Fetch BLOBs as strings instead of blob IDs */
    define('FBIRD_TEXT', 1);

    /** Fetch BLOBs as strings (alias for FBIRD_TEXT) */
    define('FBIRD_FETCH_BLOBS', 1);

    /** Fetch arrays as PHP arrays */
    define('FBIRD_FETCH_ARRAYS', 2);

    /** Return timestamps as Unix timestamps */
    define('FBIRD_UNIXTIME', 4);

    // ========================================================================
    // TRANSACTION ACCESS MODES
    // ========================================================================

    /** Read-write transaction (default) */
    define('FBIRD_WRITE', 1);

    /** Read-only transaction */
    define('FBIRD_READ', 2);

    // ========================================================================
    // TRANSACTION ISOLATION LEVELS
    // ========================================================================

    /** SNAPSHOT isolation (CONCURRENCY) */
    define('FBIRD_CONCURRENCY', 4);

    /** READ COMMITTED isolation */
    define('FBIRD_COMMITTED', 8);

    /** SNAPSHOT TABLE STABILITY isolation (CONSISTENCY) */
    define('FBIRD_CONSISTENCY', 16);

    /** Read committed: use record version */
    define('FBIRD_REC_VERSION', 64);

    /** Read committed: no record version */
    define('FBIRD_REC_NO_VERSION', 32);

    /** Read committed with READ CONSISTENCY (Firebird 4.0+) */
    define('FBIRD_READ_CONSISTENCY', 32768);

    // ========================================================================
    // TRANSACTION LOCK RESOLUTION
    // ========================================================================

    /** Wait for lock resolution */
    define('FBIRD_WAIT', 128);

    /** No wait - fail immediately on lock conflict */
    define('FBIRD_NOWAIT', 256);

    /** Lock timeout flag (used with FBIRD_WAIT) */
    define('FBIRD_LOCK_TIMEOUT', 512);

    // ========================================================================
    // TABLE RESERVATION LOCK TYPES
    // ========================================================================

    /** Shared table lock */
    define('FBIRD_LOCK_SHARED', 1024);

    /** Protected table lock */
    define('FBIRD_LOCK_PROTECTED', 2048);

    /** Exclusive table lock */
    define('FBIRD_LOCK_EXCLUSIVE', 4096);

    // ========================================================================
    // TABLE RESERVATION ACCESS TYPES
    // ========================================================================

    /** Read access for table reservation */
    define('FBIRD_LOCK_READ', 8192);

    /** Write access for table reservation */
    define('FBIRD_LOCK_WRITE', 16384);

    // ========================================================================
    // EVENT CONSTANTS
    // ========================================================================

    /** Event timeout return value */
    define('FBIRD_EVENT_TIMEOUT', -2);

    // ========================================================================
    // BLOB SEEK CONSTANTS
    // ========================================================================

    /** Seek from beginning of BLOB */
    define('FBIRD_BLOB_SEEK_SET', 0);

    /** Seek from current position */
    define('FBIRD_BLOB_SEEK_CUR', 1);

    /** Seek from end of BLOB */
    define('FBIRD_BLOB_SEEK_END', 2);

    // ========================================================================
    // SERVICE API CONSTANTS - BACKUP OPTIONS
    // ========================================================================

    define('FBIRD_BKP_IGNORE_CHECKSUMS', 1);
    define('FBIRD_BKP_IGNORE_LIMBO', 2);
    define('FBIRD_BKP_METADATA_ONLY', 4);
    define('FBIRD_BKP_NO_GARBAGE_COLLECT', 8);
    define('FBIRD_BKP_OLD_DESCRIPTIONS', 16);
    define('FBIRD_BKP_NON_TRANSPORTABLE', 32);
    define('FBIRD_BKP_CONVERT', 64);

    // ========================================================================
    // SERVICE API CONSTANTS - RESTORE OPTIONS
    // ========================================================================

    define('FBIRD_RES_DEACTIVATE_IDX', 256);
    define('FBIRD_RES_NO_SHADOW', 512);
    define('FBIRD_RES_NO_VALIDITY', 1024);
    define('FBIRD_RES_ONE_AT_A_TIME', 2048);
    define('FBIRD_RES_REPLACE', 4096);
    define('FBIRD_RES_CREATE', 8192);
    define('FBIRD_RES_USE_ALL_SPACE', 16384);

    // ========================================================================
    // SERVICE API CONSTANTS - DATABASE PROPERTIES
    // ========================================================================

    define('FBIRD_PRP_PAGE_BUFFERS', 5);
    define('FBIRD_PRP_SWEEP_INTERVAL', 6);
    define('FBIRD_PRP_SHUTDOWN_DB', 7);
    define('FBIRD_PRP_DENY_NEW_TRANSACTIONS', 10);
    define('FBIRD_PRP_DENY_NEW_ATTACHMENTS', 9);
    define('FBIRD_PRP_RESERVE_SPACE', 11);
    define('FBIRD_PRP_RES_USE_FULL', 35);
    define('FBIRD_PRP_RES', 36);
    define('FBIRD_PRP_WRITE_MODE', 12);
    define('FBIRD_PRP_WM_ASYNC', 37);
    define('FBIRD_PRP_WM_SYNC', 38);
    define('FBIRD_PRP_ACCESS_MODE', 13);
    define('FBIRD_PRP_AM_READONLY', 39);
    define('FBIRD_PRP_AM_READWRITE', 40);
    define('FBIRD_PRP_SET_SQL_DIALECT', 14);
    define('FBIRD_PRP_ACTIVATE', 256);
    define('FBIRD_PRP_DB_ONLINE', 512);

    // ========================================================================
    // SERVICE API CONSTANTS - REPAIR OPTIONS
    // ========================================================================

    define('FBIRD_RPR_CHECK_DB', 16);
    define('FBIRD_RPR_IGNORE_CHECKSUM', 32);
    define('FBIRD_RPR_KILL_SHADOWS', 64);
    define('FBIRD_RPR_MEND_DB', 4);
    define('FBIRD_RPR_VALIDATE_DB', 1);
    define('FBIRD_RPR_FULL', 128);
    define('FBIRD_RPR_SWEEP_DB', 2);

    // ========================================================================
    // SERVICE API CONSTANTS - STATISTICS
    // ========================================================================

    define('FBIRD_STS_DATA_PAGES', 1);
    define('FBIRD_STS_DB_LOG', 2);
    define('FBIRD_STS_HDR_PAGES', 4);
    define('FBIRD_STS_IDX_PAGES', 8);
    define('FBIRD_STS_SYS_RELATIONS', 16);

    // ========================================================================
    // SERVICE API CONSTANTS - SERVER INFO
    // ========================================================================

    define('FBIRD_SVC_SERVER_VERSION', 55);
    define('FBIRD_SVC_IMPLEMENTATION', 56);
    define('FBIRD_SVC_GET_ENV', 59);
    define('FBIRD_SVC_GET_ENV_LOCK', 60);
    define('FBIRD_SVC_GET_ENV_MSG', 61);
    define('FBIRD_SVC_USER_DBPATH', 58);
    define('FBIRD_SVC_SVR_DB_INFO', 50);
    define('FBIRD_SVC_GET_USERS', 68);
}

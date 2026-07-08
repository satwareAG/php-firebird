<?php
/**
 * Capability-probe helper for FB4+/FB5+ feature tests.
 *
 * Probes whether the connected server supports a given feature by attempting
 * a minimal DDL/DML operation and cleaning up. This is more robust than
 * version-string parsing because it catches config-disabled features and
 * DataTypeCompatibility modes that version detection cannot.
 *
 * Usage in --SKIPIF-- blocks:
 *   require_once __DIR__ . '/fb_version_probe.inc';
 *   if (!fb_server_supports('DECFLOAT')) die('skip DECFLOAT not supported');
 *
 * Or for inline checks in test bodies:
 *   if (!fb_server_supports('SCROLLABLE_CURSORS')) { echo "skip\n"; return; }
 *
 * Coexists with get_fb_version() from firebird.inc — not a replacement.
 * Use this for feature-specific probes; use get_fb_version() for version-
 * gated tests where capability probing is impractical.
 *
 * @package php-firebird
 * @since   13.0.0
 */

declare(strict_types=1);

require_once __DIR__ . '/config.inc';

/**
 * Probes whether the connected server supports a given feature.
 *
 * Uses capability probing (CREATE TABLE / EXECUTE BLOCK / cursor operations)
 * — never version strings. Falls back to false on any error.
 *
 * @param string $feature Feature key (see switch in _fb_probe_feature).
 * @return bool True if the feature is supported and operational.
 */
function fb_server_supports(string $feature): bool
{
    static $cache = [];

    if (isset($cache[$feature])) {
        return $cache[$feature];
    }

    return $cache[$feature] = _fb_probe_feature($feature);
}

/**
 * Connects to the test database for capability probing.
 * Uses procedural fbird_* API (works without PDO).
 */
function _fb_probe_connect()
{
    global $host, $user, $password;

    $dbEnv = getenv('FIREBIRD_DB_PATH');
    if ($dbEnv === false || $dbEnv === '') {
        $dbEnv = getenv('FIREBIRD_DATABASE');
    }
    $db = ($dbEnv !== false && $dbEnv !== '') ? $dbEnv : '/firebird/data/test.fdb';

    $hostStr = ($host !== '' && $host !== null) ? $host : 'localhost';
    $connStr = $hostStr . ':' . $db;

    return @fbird_connect($connStr, $user, $password);
}

/**
 * Dispatches to the appropriate probe for a feature key.
 * Returns false for unknown features (with a warning).
 */
function _fb_probe_feature(string $feature): bool
{
    switch ($feature) {
        /* ── FB 4.0 features ─────────────────────────────────────────── */
        case 'DECFLOAT':         return _fb_probe_decfloat();
        case 'INT128':           return _fb_probe_int128();
        case 'TIME_TZ':          return _fb_probe_time_tz();
        case 'TIMESTAMP_TZ':     return _fb_probe_timestamp_tz();
        case 'SET_BIND':         return _fb_probe_set_bind();
        case 'BATCH_DML':        return _fb_probe_batch_dml();
        case 'PACKAGES':         return _fb_probe_packages();
        case 'SQL_SECURITY':     return _fb_probe_sql_security();
        case 'READ_CONSISTENCY': return _fb_probe_read_consistency();

        /* ── FB 5.0 features ─────────────────────────────────────────── */
        case 'SCROLLABLE_CURSORS': return _fb_probe_scrollable_cursors();
        case 'PARALLEL_WORKERS':   return _fb_probe_parallel_workers();
        case 'PROFILER':           return _fb_probe_profiler();

        default:
            trigger_error("fb_server_supports(): unknown feature '{$feature}'", E_USER_WARNING);
            return false;
    }
}

/* ── FB 4.0 probes ────────────────────────────────────────────────────── */

function _fb_probe_decfloat(): bool
{
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        if (!@fbird_query($db, "RECREATE TABLE FB_PROBE_DECFLOAT (v DECFLOAT(16))")) return false;
        fbird_query($db, "INSERT INTO FB_PROBE_DECFLOAT VALUES (3.14)");
        $r = fbird_fetch_assoc(fbird_query($db, "SELECT v FROM FB_PROBE_DECFLOAT"));
        @fbird_query($db, "DROP TABLE FB_PROBE_DECFLOAT");
        return $r !== false;
    } finally {
        @fbird_close($db);
    }
}

function _fb_probe_int128(): bool
{
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        if (!@fbird_query($db, "RECREATE TABLE FB_PROBE_INT128 (v INT128)")) return false;
        fbird_query($db, "INSERT INTO FB_PROBE_INT128 VALUES (170141183460469231731687303715884105727)");
        $r = fbird_fetch_assoc(fbird_query($db, "SELECT v FROM FB_PROBE_INT128"));
        @fbird_query($db, "DROP TABLE FB_PROBE_INT128");
        return $r !== false;
    } finally {
        @fbird_close($db);
    }
}

function _fb_probe_time_tz(): bool
{
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        if (!@fbird_query($db, "RECREATE TABLE FB_PROBE_TIME_TZ (v TIME WITH TIME ZONE)")) return false;
        fbird_query($db, "INSERT INTO FB_PROBE_TIME_TZ VALUES ('10:00:00 Europe/Berlin')");
        $r = fbird_fetch_assoc(fbird_query($db, "SELECT v FROM FB_PROBE_TIME_TZ"));
        @fbird_query($db, "DROP TABLE FB_PROBE_TIME_TZ");
        return $r !== false;
    } finally {
        @fbird_close($db);
    }
}

function _fb_probe_timestamp_tz(): bool
{
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        if (!@fbird_query($db, "RECREATE TABLE FB_PROBE_TS_TZ (v TIMESTAMP WITH TIME ZONE)")) return false;
        fbird_query($db, "INSERT INTO FB_PROBE_TS_TZ VALUES ('2026-01-01 10:00:00 Europe/Berlin')");
        $r = fbird_fetch_assoc(fbird_query($db, "SELECT v FROM FB_PROBE_TS_TZ"));
        @fbird_query($db, "DROP TABLE FB_PROBE_TS_TZ");
        return $r !== false;
    } finally {
        @fbird_close($db);
    }
}

function _fb_probe_set_bind(): bool
{
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        return @fbird_query($db, "SET BIND OF DECFLOAT TO VARCHAR") !== false;
    } finally {
        @fbird_close($db);
    }
}

function _fb_probe_batch_dml(): bool
{
    if (!function_exists('fbird_batch_create')) return false;
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        $batch = @fbird_batch_create($db, "INSERT INTO RDB\$DATABASE (RDB\$FIELD_NAME) VALUES (?)");
        if (!$batch) return false;
        fbird_batch_cancel($batch);
        return true;
    } finally {
        @fbird_close($db);
    }
}

function _fb_probe_packages(): bool
{
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        @fbird_query($db, "DROP PACKAGE FB_PROBE_PKG");
        $ok = @fbird_query($db, "CREATE PACKAGE FB_PROBE_PKG AS BEGIN END");
        if ($ok) @fbird_query($db, "DROP PACKAGE FB_PROBE_PKG");
        return $ok !== false;
    } finally {
        @fbird_close($db);
    }
}

function _fb_probe_sql_security(): bool
{
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        @fbird_query($db, "DROP PROCEDURE FB_PROBE_SS");
        $ok = @fbird_query($db, "CREATE PROCEDURE FB_PROBE_SS SQL SECURITY DEFINER AS BEGIN END");
        if ($ok) @fbird_query($db, "DROP PROCEDURE FB_PROBE_SS");
        return $ok !== false;
    } finally {
        @fbird_close($db);
    }
}

function _fb_probe_read_consistency(): bool
{
    $db = _fb_probe_connect();
    if (!$db) return false;
    try {
        /* isc_tpb_read_consistency = 22, exposed via 'read_consistency' option
         * in fbird_trans() or FBIRD_READ_CONSISTENCY constant (bitmask). */
        $trans = @fbird_trans($db, ['read_consistency' => true]);
        if ($trans) {
            fbird_commit($trans);
            return true;
        }
        return false;
    } finally {
        @fbird_close($db);
    }
}

/* ── FB 5.0 probes ────────────────────────────────────────────────────── */

function _fb_probe_scrollable_cursors(): bool
{
    /* Scrollable cursors are a driver/client capability (FB5+ client +
     * server). The procedural fbird_* API exposes bidirectional fetch via
     * OOP Firebird\Statement or PDO CURSOR_SCROLL. No simple procedural
     * probe; fall back to version check. */
    require_once __DIR__ . '/firebird.inc';
    return get_fb_version() >= 5.0;
}

function _fb_probe_parallel_workers(): bool
{
    /* Parallel workers are configured via DPB, not SQL. Can't easily probe
     * DPB support without reconnecting. Fall back to version check. */
    require_once __DIR__ . '/firebird.inc';
    return get_fb_version() >= 5.0;
}

function _fb_probe_profiler(): bool
{
    /* The profiler plugin is invoked via SQL/attachments. Fall back to
     * version check. */
    require_once __DIR__ . '/firebird.inc';
    return get_fb_version() >= 5.0;
}

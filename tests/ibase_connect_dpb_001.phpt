--TEST--
ibase_connect() basic attach and SELECT 1 using defaults
--SKIPIF--
<?php
if (!extension_loaded('interbase')) {
    die('skip interbase extension not available');
}
--FILE--
<?php
require __DIR__ . '/config.inc';

// Determine correct database connection string logic (adapted from interbase.inc)
$fbDbDirEnv  = getenv('FIREBIRD_DB_DIR');
$fbDbPathEnv = getenv('FIREBIRD_DB_PATH');

if ($fbDbDirEnv !== false && $fbDbDirEnv !== '') {
    $dbDir = rtrim($fbDbDirEnv, '/');
    $tmp   = @tempnam($dbDir, 'php_ibase_test_');
    if ($tmp === false) {
        die('skip: cannot create temporary database file in FIREBIRD_DB_DIR');
    }
    @unlink($tmp);
    $host = !empty($host) ? $host . ':' . $tmp : $tmp;
} elseif ($fbDbPathEnv !== false && $fbDbPathEnv !== '') {
    $host = !empty($host) ? $host . ':' . $fbDbPathEnv : $fbDbPathEnv;
} else {
    $tmp = tempnam(sys_get_temp_dir(), "php_ibase_test");
    unlink($tmp);
    if (!empty($host)) {
        $host = $host . ':' . $tmp;
    } else {
        $host = $tmp;
    }
}

// Allow overriding host/path from the environment to target
// specific Docker engines (e.g. localhost/3052:/firebird/data/test.fdb).
$host = getenv('FB_HOST') ?: $host;

// Ensure the target database exists. In the Docker setup, FB_HOST typically
// points at a Firebird service and an absolute path inside its data volume
// (for example: firebird40:/firebird/data/test.fdb). If the attach fails
// with I/O error because the file does not exist yet, create it once using
// the same host string.
if (!@ibase_connect($host)) {
    $sql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s'", $host, $user, $password);
    $db = @ibase_query(IBASE_CREATE, $sql);
    if ($db === false) {
        die('skip: unable to create default database for ibase_connect_dpb_001');
    }
    ibase_close($db);
}

$link = ibase_connect($host);
var_dump($link !== false);

$res = ibase_query($link, 'SELECT 1 FROM RDB$DATABASE');
$row = ibase_fetch_row($res);
ibase_free_result($res);
ibase_close($link);

var_dump((int) $row[0]);
--EXPECTF--
bool(true)
int(1)

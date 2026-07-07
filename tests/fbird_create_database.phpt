--TEST--
fbird_create_database() creates a new database
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
require_once __DIR__ . '/firebird.inc';
$c = @fbird_connect($test_base, $user, $password);
if (!$c) die('skip cannot connect');
fbird_close($c);
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// Generate a unique DB path in the same directory as $test_base
// (firebird.inc already handles FIREBIRD_DB_DIR and server path resolution)
$db_dir = getenv('FIREBIRD_DB_DIR') ?: '/tmp';
$db_dir = rtrim($db_dir, '/');
$db_path = $db_dir . '/php_fbird_createdb_' . bin2hex(random_bytes(4)) . '.fdb';
$dsn = (!empty($host) ? $host . ':' : '') . $db_path;

$conn = fbird_create_database($dsn, $user, $password, 'UTF8', 8192);
if (!$conn) {
    echo "FAIL: " . fbird_errmsg() . "\n";
} else {
    echo "created\n";
    $result = fbird_query($conn, "SELECT 1 AS val FROM RDB\$DATABASE");
    $row = fbird_fetch_row($result);
    echo "query: " . $row[0] . "\n";
    fbird_free_result($result);
    fbird_drop_db($conn);
    echo "dropped\n";
}
echo "Done\n";
?>
--EXPECT--
created
query: 1
dropped
Done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

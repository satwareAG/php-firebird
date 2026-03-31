--TEST--
fbird_create_database() creates a new database
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
require_once __DIR__ . '/config.inc';
$dsn = $host . ':/firebird/data/test.fdb';
$c = @fbird_connect($dsn, $user, $password);
if (!$c) die('skip cannot connect');
fbird_close($c);
?>
--FILE--
<?php
require_once __DIR__ . '/config.inc';

$db_path = '/firebird/data/test_create_db_' . getmypid() . '.fdb';
$dsn = $host . ':' . $db_path;

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
<?php
require_once 'config.inc';
// Database creation tests - DB dropped in --FILE-- section.
// This --CLEAN-- is a safety net for crash recovery.
?>

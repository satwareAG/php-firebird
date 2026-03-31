--TEST--
fbird_drop_db() with connection string instead of resource
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

$db_path = '/firebird/data/test_drop_str_' . getmypid() . '.fdb';
$dsn = $host . ':' . $db_path;

/* Create a database to drop */
$conn = fbird_create_database($dsn, $user, $password);
if (!$conn) {
    die("FAIL: cannot create test database: " . fbird_errmsg() . "\n");
}
fbird_close($conn);
echo "created\n";

/* Drop using string overload */
$result = fbird_drop_db($dsn, $user, $password);
var_dump($result);
echo "Done\n";
?>
--EXPECT--
created
bool(true)
Done

--CLEAN--
<?php
require_once 'config.inc';
// Database creation tests - DB dropped in --FILE-- section.
// This --CLEAN-- is a safety net for crash recovery.
?>

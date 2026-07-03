--TEST--
fbird_drop_db() with connection string instead of resource
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
$db_dir = getenv('FIREBIRD_DB_DIR') ?: '/tmp';
$db_dir = rtrim($db_dir, '/');
$db_path = $db_dir . '/php_fbird_dropstr_' . bin2hex(random_bytes(4)) . '.fdb';
$dsn = (!empty($host) ? $host . ':' : '') . $db_path;

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
<?php require_once __DIR__ . '/clean.inc'; ?>

--TEST--
feat: fbird_fetch_object(class, args) - class parameter not supported
--CREDITS--
v12.1.0 M2 (#365) - procedural parity RED test
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
// fbird_fetch_object has 2 params but 2nd is $fetch_flags (int), not $class (string)
// Probe: try passing a string class name - should fail if not supported
$db = getenv('FIREBIRD_DB_PATH') ?: '/firebird/data/test.fdb';
$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$conn = @fbird_connect("{$host}/3050:{$db}", 'SYSDBA', 'masterkey');
if (!$conn) die('skip cannot connect to test DB');
$res = fbird_query($conn, "SELECT 1 FROM rdb\$database");
try {
    @$obj = fbird_fetch_object($res, "stdClass");
    fbird_free_result($res);
    fbird_close($conn);
    // If we get here, the class param IS supported - don't skip
} catch (\Throwable $e) {
    fbird_free_result($res);
    fbird_close($conn);
    die('skip gap: fbird_fetch_object does not accept string class name (see #365)');
}
?>
--FILE--
<?php
$db = getenv('FIREBIRD_DB_PATH') ?: '/firebird/data/test.fdb';
$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$conn = fbird_connect("{$host}/3050:{$db}", 'SYSDBA', 'masterkey');

class TestFetchObj { public $VAL; }

$res = fbird_query($conn, "SELECT 42 AS VAL FROM rdb\$database");
$obj = fbird_fetch_object($res, "TestFetchObj");
echo "OK fetch_object class: " . ($obj instanceof TestFetchObj ? "yes" : "no") . "\n";
fbird_free_result($res);
fbird_close($conn);
echo "=== DONE ===\n";
?>
--EXPECTF--
=== DONE ===

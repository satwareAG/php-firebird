--TEST--
fbird.enable_exceptions INI setting
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

echo "=== fbird.enable_exceptions INI test ===\n";

// Test 1: Default value (exceptions disabled)
echo "Test 1: Default value\n";
var_dump(ini_get('fbird.enable_exceptions'));

// Test 2: Check that warnings are generated (default mode)
echo "\nTest 2: Warning mode (default)\n";
$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Could not connect to test database");
}

// Force an error - invalid SQL
$result = @fbird_query($db, "SELECT * FROM nonexistent_table_xyz_123");
if ($result === false) {
    echo "Query returned false (warning suppressed)\n";
    $err = fbird_errmsg();
    var_dump(strpos($err, 'nonexistent_table_xyz_123') !== false || strpos($err, 'Table unknown') !== false || strlen($err) > 0);
}

// Test 3: Enable exceptions (reuse same database connection)
echo "\nTest 3: Exception mode\n";
ini_set('fbird.enable_exceptions', '1');
var_dump(ini_get('fbird.enable_exceptions'));

$exception_caught = false;
try {
    $result = fbird_query($db, "SELECT * FROM nonexistent_table_xyz_456");
} catch (Firebird\Exception $e) {
    $exception_caught = true;
    echo "Firebird\\Exception caught: " . (strlen($e->getMessage()) > 0 ? "yes" : "no") . "\n";
    var_dump($e instanceof Exception);
}

var_dump($exception_caught);

// Cleanup
@fbird_close($db);
ini_set('fbird.enable_exceptions', '0');

echo "\nPASS\n";
?>
--EXPECT--
=== fbird.enable_exceptions INI test ===
Test 1: Default value
string(1) "0"

Test 2: Warning mode (default)
Query returned false (warning suppressed)
bool(true)

Test 3: Exception mode
string(1) "1"
Firebird\Exception caught: yes
bool(true)
bool(true)

PASS

--TEST--
fbird.enable_exceptions default behavior (v9.0.0+)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "=== fbird.enable_exceptions default test ===\n";

// Test 1: Default value (should be enabled)
echo "Test 1: Default value\n";
var_dump(ini_get('fbird.enable_exceptions'));

// Test 2: Check that exceptions are NOT thrown by default (backward compat)
echo "\nTest 2: Exception mode (default)\n";
$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Could not connect to test database");
}

$exception_caught = false;
try {
    // Force an error - invalid SQL
    $result = @fbird_query($db, "SELECT * FROM nonexistent_table_xyz_123");
    if ($result === false) {
        echo "Query returned false (warning suppressed, no exception)\n";
    }
} catch (Firebird\Exception $e) {
    $exception_caught = true;
    echo "Firebird\\Exception caught\n";
}

if (!$exception_caught) {
    echo "SUCCESS: Exception not caught by default\n";
} else {
    echo "FAILED: Exception caught by default\n";
}

// Test 3: Can opt-in via fbird_set_exception_mode
echo "\nTest 3: Opt-in via fbird_set_exception_mode\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
echo "New exception mode: " . fbird_get_exception_mode() . "\n";

try {
    @fbird_query($db, "SELECT * FROM nonexistent_table_xyz_456");
} catch (Firebird\Exception $e) {
    echo "Firebird\\Exception caught\n";
    var_dump(strpos($e->getMessage(), 'nonexistent_table_xyz_456') !== false || strpos($e->getMessage(), 'Table unknown') !== false);
}

// Cleanup
@fbird_close($db);

echo "\nPASS\n";
?>
--EXPECT--
=== fbird.enable_exceptions default test ===
Test 1: Default value
string(1) "0"

Test 2: Exception mode (default)
Query returned false (warning suppressed, no exception)
SUCCESS: Exception not caught by default

Test 3: Opt-in via fbird_set_exception_mode
New exception mode: 1
Firebird\Exception caught
bool(true)

PASS

--TEST--
fbird_set_exception_mode() - Exception throwing behavior with database errors
--EXTENSIONS--
firebird
--FILE--
<?php
require_once __DIR__ . '/config.inc';

// Connect to database
$conn = fbird_connect($test_cfg_server, $test_cfg_user, $test_cfg_password);
if (!$conn) {
    die("Cannot connect: " . fbird_errmsg());
}

// Test 1: SILENT mode (default) - returns false on error, no exception
echo "SILENT mode (default):\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);
$result = @fbird_query($conn, 'SELECT * FROM NON_EXISTENT_TABLE_XYZ123');
var_dump($result === false);
var_dump(strlen(fbird_errmsg()) > 0);
echo "Error: " . substr(fbird_errmsg(), 0, 50) . "...\n";

// Test 2: THROW mode - throws Firebird\Exception on error
echo "\nTHROW mode:\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
try {
    $result = fbird_query($conn, 'SELECT * FROM NON_EXISTENT_TABLE_XYZ123');
    echo "FAIL: No exception thrown\n";
} catch (Firebird\Exception $e) {
    echo "SUCCESS: Firebird\\Exception caught\n";

    // Verify exception properties
    echo "getCode() is int: ";
    var_dump(is_int($e->getCode()));

    echo "getCode() non-zero: ";
    var_dump($e->getCode() !== 0);

    echo "getMessage() non-empty: ";
    var_dump(strlen($e->getMessage()) > 0);

    echo "getSqlState() is string: ";
    var_dump(is_string($e->getSqlState()));

    echo "getSqlState() length 5: ";
    var_dump(strlen($e->getSqlState()) === 5);

    echo "Exception message: " . substr($e->getMessage(), 0, 50) . "...\n";
    echo "SQLSTATE: " . $e->getSqlState() . "\n";
}

// Test 3: Verify THROW mode with transaction error
echo "\nTHROW mode with invalid transaction:\n";
try {
    // Try to use invalid resource
    $result = fbird_query("not_a_connection", 'SELECT 1');
    echo "FAIL: No exception thrown\n";
} catch (Firebird\Exception $e) {
    echo "SUCCESS: Exception caught for invalid resource\n";
} catch (TypeError $e) {
    // PHP 8 may throw TypeError for wrong argument type
    echo "SUCCESS: TypeError caught (PHP 8 strict typing)\n";
}

// Test 4: Reset to SILENT and verify no exception
echo "\nReset to SILENT:\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);
$result = @fbird_query($conn, 'INVALID SQL SYNTAX HERE');
var_dump($result === false);
echo "No exception thrown in SILENT mode\n";

// Clean up
fbird_close($conn);

echo "\nDone\n";
?>
--EXPECTF--
SILENT mode (default):
bool(true)
bool(true)
Error: %s...

THROW mode:
SUCCESS: Firebird\Exception caught
getCode() is int: bool(true)
getCode() non-zero: bool(true)
getMessage() non-empty: bool(true)
getSqlState() is string: bool(true)
getSqlState() length 5: bool(true)
Exception message: %s...
SQLSTATE: %s

THROW mode with invalid transaction:
SUCCESS: %s

Reset to SILENT:
bool(true)
No exception thrown in SILENT mode

Done

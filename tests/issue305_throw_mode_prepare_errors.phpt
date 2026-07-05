--TEST--
THROW mode: prepare failure paths throw exceptions (Issue #305)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #305:
 *   _php_fbird_prepare uses php_error_docref instead of _php_fbird_module_error.
 *   Under THROW mode, prepare failures don't throw, errcode stays 0,
 *   errmsg stays empty.
 *
 * Fix: replace php_error_docref with _php_fbird_module_error
 */

$conn = fbird_connect($test_base);
if (!$conn) die("connect failed\n");

// Enable THROW mode
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);

// Test 1: NULL query string
echo "Test 1: NULL query\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
try {
    @fbird_prepare($conn, null);
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 40) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 2: Empty query string
echo "\nTest 2: Empty query\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
try {
    @fbird_prepare($conn, '');
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 40) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 3: Invalid connection (closed)
echo "\nTest 3: Closed connection\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
$closed = fbird_connect($test_base);
fbird_close($closed);
try {
    @fbird_prepare($closed, 'SELECT 1');
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 40) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
} catch (TypeError $e) {
    echo "caught TypeError (acceptable)\n";
}

// Test 4: Verify fbird_errcode/errmsg after THROW mode failure
echo "\nTest 4: errcode/errmsg after failure\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);
try {
    @fbird_prepare($conn, '');
} catch (Firebird\Exception $e) {
    // After catching, check fbird_errcode/errmsg
    $code = fbird_errcode();
    $msg = fbird_errmsg();
    echo "fbird_errcode: " . var_export($code, true) . "\n";
    echo "fbird_errmsg non-empty: " . (strlen($msg) > 0 ? "yes" : "no") . "\n";
}

// Reset to SILENT for cleanup
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);
fbird_close($conn);
echo "\nDone\n";
?>
--EXPECTF--
Test 1: NULL query
caught: %s...
errcode: -999

Test 2: Empty query
caught: %s...
errcode: -999

Test 3: Closed connection
%s
errcode: -999

Test 4: errcode/errmsg after failure
fbird_errcode: -999
fbird_errmsg non-empty: yes

Done

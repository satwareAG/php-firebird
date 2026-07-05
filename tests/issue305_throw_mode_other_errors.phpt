--TEST--
THROW mode: connection/transaction/service/generator errors throw (Issue #305)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #305:
 *   Connection, transaction, service, generator, and sequence validation
 *   paths use php_error_docref instead of _php_fbird_module_error.
 *   Under THROW mode, these don't throw.
 *
 * Fix: replace php_error_docref with _php_fbird_module_error
 */

$conn = fbird_connect($test_base);
if (!$conn) die("connect failed\n");

// Enable THROW mode
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);

// Test 1: fbird_close with no default connection (after closing)
echo "Test 1: fbird_close no default connection\n";
fbird_close($conn);
try {
    @fbird_close();
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 35) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 2: fbird_gen_id with invalid name (too long)
echo "\nTest 2: fbird_gen_id invalid name (too long)\n";
$conn2 = fbird_connect($test_base);
try {
    @fbird_gen_id(str_repeat('A', 35), 1, $conn2);
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 35) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 3: fbird_last_insert_id with invalid sequence name
echo "\nTest 3: fbird_last_insert_id invalid sequence\n";
try {
    @fbird_last_insert_id($conn2, str_repeat('Z', 35));
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 35) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 4: fbird_savepoint with invalid name (too long)
echo "\nTest 4: fbird_savepoint invalid name\n";
$tx = fbird_trans($conn2);
try {
    @fbird_savepoint($tx, str_repeat('S', 35));
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 35) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}
fbird_rollback($tx);

// Test 5: fbird_get_limbo_transactions with invalid max_count
echo "\nTest 5: fbird_get_limbo_transactions invalid max_count\n";
try {
    @fbird_get_limbo_transactions($conn2, 0);
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 35) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 6: Verify SILENT mode still emits warnings (backward compat)
echo "\nTest 6: SILENT mode warning\n";
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);
$result = @fbird_gen_id(str_repeat('A', 35), 1, $conn2);
echo "result: " . var_export($result, false) . "\n";
echo "errmsg non-empty: " . (strlen(fbird_errmsg()) > 0 ? "yes" : "no") . "\n";
echo "errcode: " . var_export(fbird_errcode(), true) . "\n";

fbird_close($conn2);
echo "\nDone\n";
?>
--EXPECTF--
Test 1: fbird_close no default connection
caught: %s...
errcode: -999

Test 2: fbird_gen_id invalid name (too long)
caught: %s...
errcode: -999

Test 3: fbird_last_insert_id invalid sequence
caught: %s...
errcode: -999

Test 4: fbird_savepoint invalid name
caught: %s...
errcode: -999

Test 5: fbird_get_limbo_transactions invalid max_count
caught: %s...
errcode: -999

Test 6: SILENT mode warning
result: false
errmsg non-empty: yes
errcode: -999

Done

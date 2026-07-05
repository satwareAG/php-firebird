--TEST--
THROW mode: batch errors throw exceptions (Issue #305)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API requires Firebird 4.0+');
}
require_once 'firebird.inc';
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #305:
 *   fbird_batch_* validation uses php_error_docref instead of
 *   _php_fbird_module_error. Under THROW mode, invalid batch resource
 *   errors don't throw, errcode stays 0, errmsg stays empty.
 *
 * Fix: replace php_error_docref with _php_fbird_module_error
 */

$conn = fbird_connect($test_base);
if (!$conn) die("connect failed\n");

// Enable THROW mode
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);

$invalid_batch = null; // not a valid batch handle

// Test 1: fbird_batch_add with invalid handle
echo "Test 1: fbird_batch_add invalid handle\n";
try {
    @fbird_batch_add($invalid_batch, 1);
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 30) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 2: fbird_batch_execute with invalid handle
echo "\nTest 2: fbird_batch_execute invalid handle\n";
try {
    @fbird_batch_execute($invalid_batch);
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 30) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 3: fbird_batch_cancel with invalid handle
echo "\nTest 3: fbird_batch_cancel invalid handle\n";
try {
    @fbird_batch_cancel($invalid_batch);
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 30) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 4: fbird_batch_add_blob with invalid handle
echo "\nTest 4: fbird_batch_add_blob invalid handle\n";
try {
    @fbird_batch_add_blob($invalid_batch, 'test data');
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 30) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 5: fbird_batch_get_blob_alignment with invalid handle
echo "\nTest 5: fbird_batch_get_blob_alignment invalid handle\n";
try {
    @fbird_batch_get_blob_alignment($invalid_batch);
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 30) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 6: fbird_batch_set_default_bpb with invalid handle
echo "\nTest 6: fbird_batch_set_default_bpb invalid handle\n";
try {
    @fbird_batch_set_default_bpb($invalid_batch, '');
    echo "FAIL: no exception\n";
} catch (Firebird\Exception $e) {
    echo "caught: " . substr($e->getMessage(), 0, 30) . "...\n";
    echo "errcode: " . $e->getCode() . "\n";
}

// Test 7: Verify fbird_errcode/errmsg populated after catching
echo "\nTest 7: errcode/errmsg populated\n";
echo "fbird_errcode: " . var_export(fbird_errcode(), true) . "\n";
echo "fbird_errmsg non-empty: " . (strlen(fbird_errmsg()) > 0 ? "yes" : "no") . "\n";

// Reset and cleanup
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_SILENT);
fbird_close($conn);
echo "\nDone\n";
?>
--EXPECTF--
Test 1: fbird_batch_add invalid handle
caught: %s...
errcode: -999

Test 2: fbird_batch_execute invalid handle
caught: %s...
errcode: -999

Test 3: fbird_batch_cancel invalid handle
caught: %s...
errcode: -999

Test 4: fbird_batch_add_blob invalid handle
caught: %s...
errcode: -999

Test 5: fbird_batch_get_blob_alignment invalid handle
caught: %s...
errcode: -999

Test 6: fbird_batch_set_default_bpb invalid handle
caught: %s...
errcode: -999

Test 7: errcode/errmsg populated
fbird_errcode: -999
fbird_errmsg non-empty: yes

Done

--TEST--
fbird_free_query emits TypeError for invalid argument type (HF-6a)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for HF-6a (additional finding):
 *   _php_fbird_free_query_impl silently returns FALSE when argument
 *   is not a resource and not a recognized object type.
 *   Should emit TypeError for unrecognized types.
 */

$conn = fbird_connect($test_base);
if (!$conn) die("connect failed\n");

// Test 1: Pass integer (invalid type)
echo "Test 1: integer argument\n";
try {
    fbird_free_query(42);
    echo "FAIL: no TypeError, returned false silently\n";
} catch (TypeError $e) {
    echo "caught TypeError: " . substr($e->getMessage(), 0, 50) . "...\n";
}

// Test 2: Pass boolean (invalid type)
echo "\nTest 2: boolean argument\n";
try {
    fbird_free_query(true);
    echo "FAIL: no TypeError\n";
} catch (TypeError $e) {
    echo "caught TypeError: " . substr($e->getMessage(), 0, 50) . "...\n";
}

// Test 3: Pass string (invalid type)
echo "\nTest 3: string argument\n";
try {
    fbird_free_query("not_a_query");
    echo "FAIL: no TypeError\n";
} catch (TypeError $e) {
    echo "caught TypeError: " . substr($e->getMessage(), 0, 50) . "...\n";
}

// Test 4: Pass array (invalid type)
echo "\nTest 4: array argument\n";
try {
    fbird_free_query([1, 2, 3]);
    echo "FAIL: no TypeError\n";
} catch (TypeError $e) {
    echo "caught TypeError: " . substr($e->getMessage(), 0, 50) . "...\n";
}

// Test 5: Valid resource still works (regression check)
echo "\nTest 5: valid query (regression)\n";
$rs = fbird_query($conn, 'SELECT 1 FROM RDB$DATABASE');
var_dump(fbird_free_query($rs));

// Test 6: Valid Firebird\ResultSet object works (regression check)
echo "\nTest 6: Firebird\\ResultSet object (regression)\n";
$rs = fbird_query($conn, 'SELECT 1 FROM RDB$DATABASE');
var_dump(fbird_free_query($rs));

fbird_close($conn);
echo "\nDone\n";
?>
--EXPECTF--
Test 1: integer argument
caught TypeError: %s...

Test 2: boolean argument
caught TypeError: %s...

Test 3: string argument
caught TypeError: %s...

Test 4: array argument
caught TypeError: %s...

Test 5: valid query (regression)
bool(true)

Test 6: Firebird\ResultSet object (regression)
bool(true)

Done

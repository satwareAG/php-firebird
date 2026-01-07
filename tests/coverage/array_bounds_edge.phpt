--TEST--
Coverage: Array boundary conditions (fbird_query_array.c)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
?>
--FILE--
<?php
/**
 * Coverage test for array boundary conditions in fbird_query_array.c
 *
 * Tests error handling paths for:
 * - Small arrays (minimum dimensions)
 * - Large arrays (maximum practical size)
 * - NULL array handling
 * - Sparse array population
 *
 * Target: Cover error handling paths in fbird_query_array.c
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: Array Boundary Conditions ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up
@fbird_query($db, 'DROP TABLE ARRAY_BOUNDS_TEST');
@fbird_commit($db);

// Create table with various array sizes using simple syntax
$create = "
CREATE TABLE ARRAY_BOUNDS_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    TINY_ARR INTEGER[2],
    MEDIUM_ARR INTEGER[10],
    LARGE_ARR INTEGER[100]
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: Small array (2 elements)
echo "\n--- Test 1: Small array (2 elements) ---\n";
$arr = [42, 43];
$sql = "INSERT INTO ARRAY_BOUNDS_TEST (ID, TINY_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 1, $arr);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, TINY_ARR FROM ARRAY_BOUNDS_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: TINY_ARR = ";
if (is_array($row['TINY_ARR'])) {
    echo "[" . implode(", ", $row['TINY_ARR']) . "], count=" . count($row['TINY_ARR']);
}
echo "\n";
fbird_free_result($r);

// Test 2: Medium array (10 elements)
echo "\n--- Test 2: Medium array (10 elements) ---\n";
$arr = [];
for ($i = 1; $i <= 10; $i++) {
    $arr[] = $i * 10;
}
$sql = "INSERT INTO ARRAY_BOUNDS_TEST (ID, MEDIUM_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 2, $arr);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, MEDIUM_ARR FROM ARRAY_BOUNDS_TEST WHERE ID = 2");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: MEDIUM_ARR count = " . (is_array($row['MEDIUM_ARR']) ? count($row['MEDIUM_ARR']) : 0) . "\n";
if (is_array($row['MEDIUM_ARR'])) {
    $values = array_values($row['MEDIUM_ARR']);
    echo "First: {$values[0]}, Last: {$values[count($values)-1]}\n";
}
fbird_free_result($r);

// Test 3: Large array (100 elements, sparse)
echo "\n--- Test 3: Large array (100 elements) ---\n";
$arr = array_fill(0, 100, 0);
$arr[0] = 1;
$arr[49] = 50;
$arr[99] = 100;
$sql = "INSERT INTO ARRAY_BOUNDS_TEST (ID, LARGE_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 3, $arr);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, LARGE_ARR FROM ARRAY_BOUNDS_TEST WHERE ID = 3");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: LARGE_ARR count = " . (is_array($row['LARGE_ARR']) ? count($row['LARGE_ARR']) : 0) . "\n";
fbird_free_result($r);

// Test 4: NULL array column
echo "\n--- Test 4: NULL array column ---\n";
fbird_query($db, "INSERT INTO ARRAY_BOUNDS_TEST (ID) VALUES (4)");
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, TINY_ARR, MEDIUM_ARR FROM ARRAY_BOUNDS_TEST WHERE ID = 4");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: TINY_ARR is " . (is_null($row['TINY_ARR']) ? "NULL" : "NOT NULL") . "\n";
echo "ID={$row['ID']}: MEDIUM_ARR is " . (is_null($row['MEDIUM_ARR']) ? "NULL" : "NOT NULL") . "\n";
fbird_free_result($r);

// Test 5: Negative values at boundaries
echo "\n--- Test 5: Negative values at boundaries ---\n";
$arr = array_fill(0, 10, 0);
$arr[0] = -2147483648;
$arr[9] = 2147483647;
$sql = "INSERT INTO ARRAY_BOUNDS_TEST (ID, MEDIUM_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 5, $arr);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, MEDIUM_ARR FROM ARRAY_BOUNDS_TEST WHERE ID = 5");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
if (is_array($row['MEDIUM_ARR'])) {
    $values = array_values($row['MEDIUM_ARR']);
    echo "ID={$row['ID']}: First={$values[0]}, Last={$values[count($values)-1]}\n";
}
fbird_free_result($r);

// Final count
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM ARRAY_BOUNDS_TEST");
$row = fbird_fetch_assoc($r);
echo "\n--- Total rows: " . $row['CNT'] . " ---\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE ARRAY_BOUNDS_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Array Boundary Conditions ===
Test table created

--- Test 1: Small array (2 elements) ---
ID=1: TINY_ARR = [42, 43], count=2

--- Test 2: Medium array (10 elements) ---
ID=2: MEDIUM_ARR count = 10
First: 10, Last: 100

--- Test 3: Large array (100 elements) ---
ID=3: LARGE_ARR count = 100

--- Test 4: NULL array column ---
ID=4: TINY_ARR is NULL
ID=4: MEDIUM_ARR is NULL

--- Test 5: Negative values at boundaries ---
ID=5: First=-2147483648, Last=2147483647

--- Total rows: 5 ---

--- Cleanup ---

=== Test Complete ===

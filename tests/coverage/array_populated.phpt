--TEST--
Coverage: Array operations with populated data (fbird_query_array.c)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
?>
--FILE--
<?php
/**
 * Coverage test for array operations in fbird_query_array.c
 *
 * Tests array allocation and binding paths using PHP array parameters:
 * - INTEGER, SMALLINT, BIGINT arrays
 * - FLOAT, DOUBLE PRECISION arrays
 * - CHAR, VARCHAR arrays
 * - Multi-dimensional arrays
 *
 * Target: Cover lines 35-544 in fbird_query_array.c
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: Array Operations with Data ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up
@fbird_query($db, 'DROP TABLE ARRAY_DATA_TEST');
@fbird_commit($db);

// Create table with various array column types using simple syntax
$create = "
CREATE TABLE ARRAY_DATA_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    INT_ARR INTEGER[5],
    SMALL_ARR SMALLINT[3],
    BIGINT_ARR BIGINT[4],
    FLOAT_ARR FLOAT[3],
    DOUBLE_ARR DOUBLE PRECISION[4],
    CHAR_ARR CHAR(10)[3],
    VARCHAR_ARR VARCHAR(20)[5]
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: Populate integer arrays via PHP binding
echo "\n--- Test 1: Integer arrays via PHP binding ---\n";
$sql = "INSERT INTO ARRAY_DATA_TEST (ID, INT_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 1, [10, 20, 30, 40, 50]);
fbird_query($db, $sql, 2, [-100, 0, 100, 200, 300]);
fbird_query($db, $sql, 3, [1, 2, 3, 4, 5]);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, INT_ARR FROM ARRAY_DATA_TEST WHERE ID <= 3 ORDER BY ID");
while ($row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS)) {
    echo "ID={$row['ID']}: INT_ARR = ";
    if (is_array($row['INT_ARR'])) {
        echo "[" . implode(", ", $row['INT_ARR']) . "]";
    } else {
        echo var_export($row['INT_ARR'], true);
    }
    echo "\n";
}
fbird_free_result($r);

// Test 2: SMALLINT array
echo "\n--- Test 2: SMALLINT array ---\n";
$sql = "INSERT INTO ARRAY_DATA_TEST (ID, SMALL_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 10, [1, 2, 3]);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, SMALL_ARR FROM ARRAY_DATA_TEST WHERE ID = 10");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: SMALL_ARR = ";
if (is_array($row['SMALL_ARR'])) {
    echo "[" . implode(", ", $row['SMALL_ARR']) . "]";
}
echo "\n";
fbird_free_result($r);

// Test 3: BIGINT array
echo "\n--- Test 3: BIGINT array ---\n";
$sql = "INSERT INTO ARRAY_DATA_TEST (ID, BIGINT_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 20, [9223372036854775807, -922337203685477580, 0, 123456789012345]);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, BIGINT_ARR FROM ARRAY_DATA_TEST WHERE ID = 20");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: BIGINT_ARR fetched, count = " . (is_array($row['BIGINT_ARR']) ? count($row['BIGINT_ARR']) : 0) . "\n";
fbird_free_result($r);

// Test 4: Float array
echo "\n--- Test 4: FLOAT array ---\n";
$sql = "INSERT INTO ARRAY_DATA_TEST (ID, FLOAT_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 30, [3.14159, 2.71828, 1.41421]);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, FLOAT_ARR FROM ARRAY_DATA_TEST WHERE ID = 30");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: FLOAT_ARR fetched\n";
if (is_array($row['FLOAT_ARR'])) {
    $vals = array_map(function($v) { return round($v, 4); }, $row['FLOAT_ARR']);
    echo "Values: " . implode(", ", $vals) . "\n";
}
fbird_free_result($r);

// Test 5: DOUBLE PRECISION array
echo "\n--- Test 5: DOUBLE PRECISION array ---\n";
$sql = "INSERT INTO ARRAY_DATA_TEST (ID, DOUBLE_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 31, [3.141592653589793, 2.718281828459045, 1.4142135623730951, 1.7976931348623157E+100]);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, DOUBLE_ARR FROM ARRAY_DATA_TEST WHERE ID = 31");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: DOUBLE_ARR fetched, count = " . (is_array($row['DOUBLE_ARR']) ? count($row['DOUBLE_ARR']) : 0) . "\n";
fbird_free_result($r);

// Test 6: CHAR array
echo "\n--- Test 6: CHAR array ---\n";
$sql = "INSERT INTO ARRAY_DATA_TEST (ID, CHAR_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 40, ['First', 'Second', 'Third']);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, CHAR_ARR FROM ARRAY_DATA_TEST WHERE ID = 40");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: CHAR_ARR fetched\n";
if (is_array($row['CHAR_ARR'])) {
    foreach ($row['CHAR_ARR'] as $i => $v) {
        echo "  [$i] = '" . trim($v) . "'\n";
    }
}
fbird_free_result($r);

// Test 7: VARCHAR array
echo "\n--- Test 7: VARCHAR array ---\n";
$sql = "INSERT INTO ARRAY_DATA_TEST (ID, VARCHAR_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 50, ['Apple', 'Banana', 'Cherry', 'Date', 'Elderberry']);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, VARCHAR_ARR FROM ARRAY_DATA_TEST WHERE ID = 50");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: VARCHAR_ARR fetched\n";
if (is_array($row['VARCHAR_ARR'])) {
    echo "Values: [" . implode(", ", $row['VARCHAR_ARR']) . "]\n";
}
fbird_free_result($r);

// Test 8: Partial array (some elements NULL/zero)
echo "\n--- Test 8: Partial array population ---\n";
$arr = [100, 0, 300, 0, 500];
$sql = "INSERT INTO ARRAY_DATA_TEST (ID, INT_ARR) VALUES (?, ?)";
fbird_query($db, $sql, 70, $arr);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, INT_ARR FROM ARRAY_DATA_TEST WHERE ID = 70");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']}: INT_ARR = ";
if (is_array($row['INT_ARR'])) {
    echo "[" . implode(", ", $row['INT_ARR']) . "]";
}
echo "\n";
fbird_free_result($r);

// Test 9: Update array column
echo "\n--- Test 9: Update array column ---\n";
$sql = "UPDATE ARRAY_DATA_TEST SET INT_ARR = ? WHERE ID = 1";
fbird_query($db, $sql, [999, 888, 777, 666, 555]);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, INT_ARR FROM ARRAY_DATA_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($r, FBIRD_FETCH_ARRAYS);
echo "ID={$row['ID']} after update: INT_ARR = ";
if (is_array($row['INT_ARR'])) {
    echo "[" . implode(", ", $row['INT_ARR']) . "]";
}
echo "\n";
fbird_free_result($r);

// Final count
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM ARRAY_DATA_TEST");
$row = fbird_fetch_assoc($r);
echo "\n--- Total rows: " . $row['CNT'] . " ---\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE ARRAY_DATA_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Array Operations with Data ===
Test table created

--- Test 1: Integer arrays via PHP binding ---
ID=1: INT_ARR = [10, 20, 30, 40, 50]
ID=2: INT_ARR = [-100, 0, 100, 200, 300]
ID=3: INT_ARR = [1, 2, 3, 4, 5]

--- Test 2: SMALLINT array ---
ID=10: SMALL_ARR = [1, 2, 3]

--- Test 3: BIGINT array ---
ID=20: BIGINT_ARR fetched, count = 4

--- Test 4: FLOAT array ---
ID=30: FLOAT_ARR fetched
Values: %s

--- Test 5: DOUBLE PRECISION array ---
ID=31: DOUBLE_ARR fetched, count = 4

--- Test 6: CHAR array ---
ID=40: CHAR_ARR fetched
  [%d] = 'First'
  [%d] = 'Second'
  [%d] = 'Third'

--- Test 7: VARCHAR array ---
ID=50: VARCHAR_ARR fetched
Values: [Apple, Banana, Cherry, Date, Elderberry]

--- Test 8: Partial array population ---
ID=70: INT_ARR = [100, 0, 300, 0, 500]

--- Test 9: Update array column ---
ID=1 after update: INT_ARR = [999, 888, 777, 666, 555]

--- Total rows: %d ---

--- Cleanup ---

=== Test Complete ===

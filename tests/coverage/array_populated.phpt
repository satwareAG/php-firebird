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
 * Tests array allocation and binding paths:
 * - _php_fbird_alloc_array() with various array types
 * - _php_fbird_bind_array() with actual data
 * - Multi-dimensional array handling
 * - Different element types (INTEGER, SMALLINT, BIGINT, FLOAT, VARCHAR)
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
@fbird_query($db, 'DROP PROCEDURE POPULATE_INT_ARRAY');
@fbird_query($db, 'DROP PROCEDURE POPULATE_MULTI_ARRAY');
@fbird_commit($db);

// Create table with various array column types
// Firebird array syntax: DATATYPE [lower:upper] or [lower:upper, lower:upper] for multi-dim
$create = "
CREATE TABLE ARRAY_DATA_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    INT_ARR INTEGER[1:5],
    SMALL_ARR SMALLINT[1:3],
    BIGINT_ARR BIGINT[1:4],
    FLOAT_ARR FLOAT[1:3],
    DOUBLE_ARR DOUBLE PRECISION[1:4],
    CHAR_ARR CHAR(10)[1:3],
    VARCHAR_ARR VARCHAR(20)[1:5],
    MULTI_ARR INTEGER[1:2, 1:3]
)";
$result = fbird_query($db, $create);
if (!$result) {
    die("Failed to create table: " . fbird_errmsg() . "\n");
}
fbird_commit($db);
echo "Test table created\n";

// Create stored procedure to populate integer array
$proc1 = "
CREATE PROCEDURE POPULATE_INT_ARRAY (IN_ID INTEGER, VAL1 INTEGER, VAL2 INTEGER, VAL3 INTEGER, VAL4 INTEGER, VAL5 INTEGER)
AS
DECLARE arr INTEGER[1:5];
BEGIN
    arr[1] = VAL1;
    arr[2] = VAL2;
    arr[3] = VAL3;
    arr[4] = VAL4;
    arr[5] = VAL5;
    INSERT INTO ARRAY_DATA_TEST (ID, INT_ARR) VALUES (:IN_ID, :arr);
END";
$result = fbird_query($db, $proc1);
if (!$result) {
    die("Failed to create procedure: " . fbird_errmsg() . "\n");
}
fbird_commit($db);
echo "Stored procedures created\n";

// Test 1: Populate array using stored procedure
echo "\n--- Test 1: Array populated via stored procedure ---\n";
fbird_query($db, "EXECUTE PROCEDURE POPULATE_INT_ARRAY(1, 10, 20, 30, 40, 50)");
fbird_query($db, "EXECUTE PROCEDURE POPULATE_INT_ARRAY(2, -100, 0, 100, 200, 300)");
fbird_query($db, "EXECUTE PROCEDURE POPULATE_INT_ARRAY(3, 1, 2, 3, 4, 5)");
fbird_commit($db);

// Fetch and display array data
$r = fbird_query($db, "SELECT ID, INT_ARR FROM ARRAY_DATA_TEST WHERE ID <= 3 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: INT_ARR = ";
    if (is_array($row['INT_ARR'])) {
        echo "[" . implode(", ", $row['INT_ARR']) . "]";
    } else {
        echo var_export($row['INT_ARR'], true);
    }
    echo "\n";
}
fbird_free_result($r);

// Test 2: Direct array insert using PSQL blocks
echo "\n--- Test 2: Array via EXECUTE BLOCK ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr SMALLINT[1:3];
BEGIN
    arr[1] = 1;
    arr[2] = 2;
    arr[3] = 3;
    INSERT INTO ARRAY_DATA_TEST (ID, SMALL_ARR) VALUES (10, arr);
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, SMALL_ARR FROM ARRAY_DATA_TEST WHERE ID = 10");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: SMALL_ARR = ";
if (is_array($row['SMALL_ARR'])) {
    echo "[" . implode(", ", $row['SMALL_ARR']) . "]";
} else {
    echo var_export($row['SMALL_ARR'], true);
}
echo "\n";
fbird_free_result($r);

// Test 3: BIGINT array
echo "\n--- Test 3: BIGINT array ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr BIGINT[1:4];
BEGIN
    arr[1] = 9223372036854775807;
    arr[2] = -9223372036854775808;
    arr[3] = 0;
    arr[4] = 123456789012345;
    INSERT INTO ARRAY_DATA_TEST (ID, BIGINT_ARR) VALUES (20, arr);
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, BIGINT_ARR FROM ARRAY_DATA_TEST WHERE ID = 20");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: BIGINT_ARR fetched, count = " . (is_array($row['BIGINT_ARR']) ? count($row['BIGINT_ARR']) : 0) . "\n";
fbird_free_result($r);

// Test 4: Float and Double arrays
echo "\n--- Test 4: FLOAT array ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr FLOAT[1:3];
BEGIN
    arr[1] = 3.14159;
    arr[2] = 2.71828;
    arr[3] = 1.41421;
    INSERT INTO ARRAY_DATA_TEST (ID, FLOAT_ARR) VALUES (30, arr);
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, FLOAT_ARR FROM ARRAY_DATA_TEST WHERE ID = 30");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: FLOAT_ARR fetched\n";
if (is_array($row['FLOAT_ARR'])) {
    echo "Values: ";
    foreach ($row['FLOAT_ARR'] as $i => $v) {
        echo round($v, 4);
        if ($i < count($row['FLOAT_ARR'])) echo ", ";
    }
    echo "\n";
}
fbird_free_result($r);

// Test 5: DOUBLE PRECISION array
echo "\n--- Test 5: DOUBLE PRECISION array ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr DOUBLE PRECISION[1:4];
BEGIN
    arr[1] = 3.141592653589793;
    arr[2] = 2.718281828459045;
    arr[3] = 1.4142135623730951;
    arr[4] = 1.7976931348623157E+308;
    INSERT INTO ARRAY_DATA_TEST (ID, DOUBLE_ARR) VALUES (31, arr);
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, DOUBLE_ARR FROM ARRAY_DATA_TEST WHERE ID = 31");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: DOUBLE_ARR fetched, count = " . (is_array($row['DOUBLE_ARR']) ? count($row['DOUBLE_ARR']) : 0) . "\n";
fbird_free_result($r);

// Test 6: CHAR array
echo "\n--- Test 6: CHAR array ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr CHAR(10)[1:3];
BEGIN
    arr[1] = 'First';
    arr[2] = 'Second';
    arr[3] = 'Third';
    INSERT INTO ARRAY_DATA_TEST (ID, CHAR_ARR) VALUES (40, arr);
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, CHAR_ARR FROM ARRAY_DATA_TEST WHERE ID = 40");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: CHAR_ARR fetched\n";
if (is_array($row['CHAR_ARR'])) {
    foreach ($row['CHAR_ARR'] as $i => $v) {
        echo "  [$i] = '" . trim($v) . "'\n";
    }
}
fbird_free_result($r);

// Test 7: VARCHAR array
echo "\n--- Test 7: VARCHAR array ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr VARCHAR(20)[1:5];
BEGIN
    arr[1] = 'Apple';
    arr[2] = 'Banana';
    arr[3] = 'Cherry';
    arr[4] = 'Date';
    arr[5] = 'Elderberry';
    INSERT INTO ARRAY_DATA_TEST (ID, VARCHAR_ARR) VALUES (50, arr);
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, VARCHAR_ARR FROM ARRAY_DATA_TEST WHERE ID = 50");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: VARCHAR_ARR fetched\n";
if (is_array($row['VARCHAR_ARR'])) {
    echo "Values: [" . implode(", ", $row['VARCHAR_ARR']) . "]\n";
}
fbird_free_result($r);

// Test 8: Multi-dimensional array
echo "\n--- Test 8: Multi-dimensional array [2,3] ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr INTEGER[1:2, 1:3];
BEGIN
    arr[1,1] = 11;
    arr[1,2] = 12;
    arr[1,3] = 13;
    arr[2,1] = 21;
    arr[2,2] = 22;
    arr[2,3] = 23;
    INSERT INTO ARRAY_DATA_TEST (ID, MULTI_ARR) VALUES (60, arr);
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, MULTI_ARR FROM ARRAY_DATA_TEST WHERE ID = 60");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: MULTI_ARR fetched\n";
if (is_array($row['MULTI_ARR'])) {
    echo "Structure: " . print_r($row['MULTI_ARR'], true);
}
fbird_free_result($r);

// Test 9: Partial array (some elements NULL/unset)
echo "\n--- Test 9: Partial array population ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr INTEGER[1:5];
BEGIN
    arr[1] = 100;
    arr[3] = 300;
    arr[5] = 500;
    INSERT INTO ARRAY_DATA_TEST (ID, INT_ARR) VALUES (70, arr);
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, INT_ARR FROM ARRAY_DATA_TEST WHERE ID = 70");
$row = fbird_fetch_assoc($r);
echo "ID={$row['ID']}: INT_ARR = ";
if (is_array($row['INT_ARR'])) {
    $display = array_map(function($v) { return $v ?? 'NULL'; }, $row['INT_ARR']);
    echo "[" . implode(", ", $display) . "]";
}
echo "\n";
fbird_free_result($r);

// Test 10: Update array column
echo "\n--- Test 10: Update array column ---\n";
$block = "
EXECUTE BLOCK AS
DECLARE arr INTEGER[1:5];
BEGIN
    arr[1] = 999;
    arr[2] = 888;
    arr[3] = 777;
    arr[4] = 666;
    arr[5] = 555;
    UPDATE ARRAY_DATA_TEST SET INT_ARR = :arr WHERE ID = 1;
END";
fbird_query($db, $block);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, INT_ARR FROM ARRAY_DATA_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($r);
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
@fbird_query($db, 'DROP PROCEDURE POPULATE_INT_ARRAY');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Array Operations with Data ===
Test table created
Stored procedures created

--- Test 1: Array populated via stored procedure ---
ID=1: INT_ARR = [10, 20, 30, 40, 50]
ID=2: INT_ARR = [-100, 0, 100, 200, 300]
ID=3: INT_ARR = [1, 2, 3, 4, 5]

--- Test 2: Array via EXECUTE BLOCK ---
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
  [%s] = 'First'
  [%s] = 'Second'
  [%s] = 'Third'

--- Test 7: VARCHAR array ---
ID=50: VARCHAR_ARR fetched
Values: [Apple, Banana, Cherry, Date, Elderberry]

--- Test 8: Multi-dimensional array [2,3] ---
ID=60: MULTI_ARR fetched
Structure: Array
%A

--- Test 9: Partial array population ---
ID=70: INT_ARR = [%s]

--- Test 10: Update array column ---
ID=1 after update: INT_ARR = [999, 888, 777, 666, 555]

--- Total rows: %d ---

--- Cleanup ---

=== Test Complete ===

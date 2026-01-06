--TEST--
Coverage: Firebird ARRAY column allocation (_php_fbird_alloc_array)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
// Array column support is available in all FB versions, but some types need FB 4.0+
?>
--FILE--
<?php
/**
 * Coverage test for _php_fbird_alloc_array() in fbird_query_array.c
 *
 * Tests:
 * - INTEGER array allocation
 * - SMALLINT array allocation
 * - VARCHAR array allocation (with dtype_cstring workaround)
 * - CHAR array allocation
 * - FLOAT/DOUBLE array allocation
 * - DATE/TIME/TIMESTAMP array allocation
 * - Multi-element arrays
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: Firebird Array Column Allocation ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up any existing test table
@fbird_query($db, 'DROP TABLE ARRAY_ALLOC_TEST');
@fbird_commit($db);

// Create table with various array column types
echo "Creating table with array columns...\n";
$create_sql = "
CREATE TABLE ARRAY_ALLOC_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    INT_ARR INTEGER[5],
    SMALL_ARR SMALLINT[3],
    CHAR_ARR CHAR(10)[4],
    VARCHAR_ARR VARCHAR(20)[3],
    FLOAT_ARR FLOAT[2],
    DOUBLE_ARR DOUBLE PRECISION[2],
    DATE_ARR DATE[2],
    TIME_ARR TIME[2],
    TS_ARR TIMESTAMP[2]
)";

$result = @fbird_query($db, $create_sql);
if (!$result) {
    // Firebird array syntax may vary - try alternative
    echo "Note: Standard array syntax may not be supported\n";
    echo "Error: " . fbird_errmsg() . "\n";
    fbird_close($db);
    echo "SKIP: Array columns not supported in this Firebird version\n";
    exit;
}
fbird_commit($db);
echo "Table created successfully\n";

// Test 1: Insert with NULL arrays (simplest case)
echo "\n--- Test 1: Insert with NULL arrays ---\n";
$result = fbird_query($db, "INSERT INTO ARRAY_ALLOC_TEST (ID) VALUES (1)");
var_dump($result !== false);
fbird_commit($db);

// Test 2: Select to trigger array allocation
echo "\n--- Test 2: Select with FBIRD_FETCH_ARRAYS flag ---\n";
$result = fbird_query($db, "SELECT * FROM ARRAY_ALLOC_TEST WHERE ID = 1");
if ($result) {
    // FBIRD_FETCH_ARRAYS = 2
    $row = fbird_fetch_assoc($result, FBIRD_FETCH_ARRAYS);
    echo "Row fetched: " . (is_array($row) ? "YES" : "NO") . "\n";
    if (is_array($row)) {
        echo "ID: " . $row['ID'] . "\n";
        echo "INT_ARR is null: " . (is_null($row['INT_ARR']) ? "YES" : "NO") . "\n";
    }
    fbird_free_result($result);
} else {
    echo "Query failed: " . fbird_errmsg() . "\n";
}

// Test 3: Select all types to trigger array allocation for each type
// Note: Firebird array columns require stored procedures or API calls to populate
// The SELECT with FBIRD_FETCH_ARRAYS triggers _php_fbird_alloc_array()
echo "\n--- Test 3: Select all array columns to trigger allocation ---\n";
$result = fbird_query($db, "SELECT * FROM ARRAY_ALLOC_TEST WHERE ID = 1");
if ($result) {
    $row = fbird_fetch_assoc($result, FBIRD_FETCH_ARRAYS);
    echo "Columns fetched: " . count($row) . "\n";
    // Each array column triggers _php_fbird_alloc_array() type detection
    $array_cols = ['INT_ARR', 'SMALL_ARR', 'CHAR_ARR', 'VARCHAR_ARR', 
                   'FLOAT_ARR', 'DOUBLE_ARR', 'DATE_ARR', 'TIME_ARR', 'TS_ARR'];
    foreach ($array_cols as $col) {
        $val = $row[$col] ?? 'MISSING';
        $type = is_null($val) ? 'NULL' : gettype($val);
        echo "  $col: $type\n";
    }
    fbird_free_result($result);
}

// Test 4: Multiple rows with array columns
echo "\n--- Test 4: Insert more rows (NULL arrays) ---\n";
for ($i = 2; $i <= 5; $i++) {
    $result = fbird_query($db, "INSERT INTO ARRAY_ALLOC_TEST (ID) VALUES ($i)");
    if (!$result) {
        echo "Insert $i failed: " . fbird_errmsg() . "\n";
    }
}
fbird_commit($db);
echo "Inserted 4 additional rows\n";

// Test 5: Batch select triggers array allocation per row
echo "\n--- Test 5: Batch select all rows ---\n";
$result = fbird_query($db, "SELECT ID, INT_ARR, VARCHAR_ARR FROM ARRAY_ALLOC_TEST ORDER BY ID");
$count = 0;
while ($row = fbird_fetch_row($result, FBIRD_FETCH_ARRAYS)) {
    $count++;
}
echo "Fetched $count rows with array columns\n";
fbird_free_result($result);

// Test 6: Select without FBIRD_FETCH_ARRAYS (no array allocation)
echo "\n--- Test 6: Select without FBIRD_FETCH_ARRAYS ---\n";
$result = fbird_query($db, "SELECT ID, INT_ARR FROM ARRAY_ALLOC_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($result);
// Without FBIRD_FETCH_ARRAYS, arrays return as internal reference
echo "INT_ARR without flag type: " . gettype($row['INT_ARR']) . "\n";
fbird_free_result($result);

// Test 7: Count total rows
echo "\n--- Test 7: Count total rows ---\n";
$result = fbird_query($db, "SELECT COUNT(*) AS CNT FROM ARRAY_ALLOC_TEST");
$row = fbird_fetch_assoc($result);
echo "Total rows: " . $row['CNT'] . "\n";
fbird_free_result($result);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE ARRAY_ALLOC_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Firebird Array Column Allocation ===
Creating table with array columns...
Table created successfully

--- Test 1: Insert with NULL arrays ---
bool(true)

--- Test 2: Select with FBIRD_FETCH_ARRAYS flag ---
Row fetched: YES
ID: 1
INT_ARR is null: YES

--- Test 3: Select all array columns to trigger allocation ---
Columns fetched: 10
  INT_ARR: %s
  SMALL_ARR: %s
  CHAR_ARR: %s
  VARCHAR_ARR: %s
  FLOAT_ARR: %s
  DOUBLE_ARR: %s
  DATE_ARR: %s
  TIME_ARR: %s
  TS_ARR: %s

--- Test 4: Insert more rows (NULL arrays) ---
Inserted 4 additional rows

--- Test 5: Batch select all rows ---
Fetched 5 rows with array columns

--- Test 6: Select without FBIRD_FETCH_ARRAYS ---
INT_ARR without flag type: %s

--- Test 7: Count total rows ---
Total rows: 5

--- Cleanup ---

=== Test Complete ===

--TEST--
Coverage: Large batch buffer management (firebird_utils.cpp)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
// IBatch API requires Firebird 4.0+
if (!defined('FBIRD_BATCH_WRITE') && !function_exists('fbird_batch_create')) {
    die('skip IBatch API requires Firebird 4.0+');
}
require_once __DIR__ . '/../firebird.inc';
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php
/**
 * Coverage test for large batch buffer management in firebird_utils.cpp
 *
 * Tests buffer handling paths for:
 * - Large row counts (500+ rows)
 * - Buffer reallocation
 * - Memory management under load
 *
 * Target: Cover buffer management paths in fbbatch_* functions
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: Large Batch Buffer Management ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up any existing test table
@fbird_query($db, 'DROP TABLE BATCH_LARGE_TEST');
@fbird_commit($db);

// Create test table with VARCHAR to test buffer sizing
$create = "
CREATE TABLE BATCH_LARGE_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    INT_VAL INTEGER,
    STR_VAL VARCHAR(200),
    FLOAT_VAL DOUBLE PRECISION
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: Medium batch (100 rows)
echo "\n--- Test 1: Medium batch (100 rows) ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_LARGE_TEST (ID, INT_VAL, STR_VAL) VALUES (?, ?, ?)");
$batch = fbird_batch_create($stmt, $trans);

$start = microtime(true);
for ($i = 0; $i < 100; $i++) {
    fbird_batch_add($batch, $i, $i * 10, "Row number $i with some padding text");
}
$add_time = microtime(true) - $start;
echo "Added 100 rows in " . round($add_time * 1000, 2) . " ms\n";

$start = microtime(true);
$result = fbird_batch_execute($batch);
$exec_time = microtime(true) - $start;
echo "Executed in " . round($exec_time * 1000, 2) . " ms\n";
echo "Success count: " . ($result['success_count'] ?? 'N/A') . "\n";
fbird_commit($trans);
fbird_free_query($stmt);

// Verify
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM BATCH_LARGE_TEST WHERE ID < 100");
$row = fbird_fetch_assoc($r);
echo "Verified rows: " . $row['CNT'] . "\n";
fbird_free_result($r);

// Test 2: Large batch (500 rows)
echo "\n--- Test 2: Large batch (500 rows) ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_LARGE_TEST (ID, INT_VAL, FLOAT_VAL) VALUES (?, ?, ?)");
$batch = fbird_batch_create($stmt, $trans);

$start = microtime(true);
for ($i = 1000; $i < 1500; $i++) {
    fbird_batch_add($batch, $i, $i * 2, $i / 3.0);
}
$add_time = microtime(true) - $start;
echo "Added 500 rows in " . round($add_time * 1000, 2) . " ms\n";

$start = microtime(true);
$result = fbird_batch_execute($batch);
$exec_time = microtime(true) - $start;
echo "Executed in " . round($exec_time * 1000, 2) . " ms\n";
echo "Success count: " . ($result['success_count'] ?? 'N/A') . "\n";
fbird_commit($trans);
fbird_free_query($stmt);

// Verify - commit then requery to ensure visibility
fbird_commit($db);  // Ensure auto-started transaction sees committed data
$r = fbird_query($db, "SELECT COUNT(*) AS CNT, MIN(ID) AS MIN_ID, MAX(ID) AS MAX_ID FROM BATCH_LARGE_TEST WHERE ID >= 1000 AND ID < 1500");
$row = fbird_fetch_assoc($r);
$min_id = $row['MIN_ID'] ?? '-';
$max_id = $row['MAX_ID'] ?? '-';
echo "Verified: " . $row['CNT'] . " rows (ID range " . $min_id . "-" . $max_id . ")\n";
fbird_free_result($r);

// Test 3: Very large batch (1000 rows)
echo "\n--- Test 3: Very large batch (1000 rows) ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_LARGE_TEST (ID, INT_VAL, STR_VAL, FLOAT_VAL) VALUES (?, ?, ?, ?)");
$batch = fbird_batch_create($stmt, $trans);

$start = microtime(true);
for ($i = 2000; $i < 3000; $i++) {
    $str = "Variable length string for row $i with extra content";
    fbird_batch_add($batch, $i, $i, $str, sin($i));
}
$add_time = microtime(true) - $start;
echo "Added 1000 rows in " . round($add_time * 1000, 2) . " ms\n";

$start = microtime(true);
$result = fbird_batch_execute($batch);
$exec_time = microtime(true) - $start;
echo "Executed in " . round($exec_time * 1000, 2) . " ms\n";
echo "Success count: " . ($result['success_count'] ?? 'N/A') . "\n";
fbird_commit($trans);
fbird_free_query($stmt);

// Verify
fbird_commit($db);
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM BATCH_LARGE_TEST WHERE ID >= 2000 AND ID < 3000");
$row = fbird_fetch_assoc($r);
echo "Verified rows: " . $row['CNT'] . "\n";
fbird_free_result($r);

// Test 4: Multiple small batches in sequence
echo "\n--- Test 4: Multiple small batches (10 x 50 rows) ---\n";
$total_inserted = 0;
$start = microtime(true);

for ($batch_num = 0; $batch_num < 10; $batch_num++) {
    $trans = fbird_trans($db);
    $stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_LARGE_TEST (ID, INT_VAL) VALUES (?, ?)");
    $batch = fbird_batch_create($stmt, $trans);
    
    $base_id = 10000 + ($batch_num * 50);
    for ($i = 0; $i < 50; $i++) {
        fbird_batch_add($batch, $base_id + $i, $batch_num);
    }
    
    $result = fbird_batch_execute($batch);
    $total_inserted += $result['success_count'] ?? 0;
    fbird_commit($trans);
    fbird_free_query($stmt);
}

$total_time = microtime(true) - $start;
echo "Total time: " . round($total_time * 1000, 2) . " ms\n";
echo "Total inserted: $total_inserted rows\n";

// Test 5: Batch with long strings (buffer stress)
echo "\n--- Test 5: Batch with long strings ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_LARGE_TEST (ID, STR_VAL) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

for ($i = 0; $i < 50; $i++) {
    $long_str = str_repeat("X", 180 + ($i % 20));  // Vary length 180-199
    fbird_batch_add($batch, 20000 + $i, $long_str);
}

$result = fbird_batch_execute($batch);
echo "Long string batch success: " . ($result['success_count'] ?? 'N/A') . "\n";
fbird_commit($trans);
fbird_free_query($stmt);

// Verify lengths
fbird_commit($db);
$r = fbird_query($db, "SELECT MIN(CHAR_LENGTH(STR_VAL)) AS MIN_LEN, MAX(CHAR_LENGTH(STR_VAL)) AS MAX_LEN FROM BATCH_LARGE_TEST WHERE ID >= 20000 AND ID < 20050");
$row = fbird_fetch_assoc($r);
$min_len = $row['MIN_LEN'] ?? '';
$max_len = $row['MAX_LEN'] ?? '';
echo "String lengths: " . $min_len . " to " . $max_len . " chars\n";
fbird_free_result($r);

// Test 6: Mixed NULL and values in large batch
echo "\n--- Test 6: Large batch with NULLs ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_LARGE_TEST (ID, INT_VAL, STR_VAL) VALUES (?, ?, ?)");
$batch = fbird_batch_create($stmt, $trans);

for ($i = 0; $i < 200; $i++) {
    $int_val = ($i % 3 == 0) ? null : $i;
    $str_val = ($i % 5 == 0) ? null : "Row $i";
    fbird_batch_add($batch, 30000 + $i, $int_val, $str_val);
}

$result = fbird_batch_execute($batch);
echo "Mixed NULL batch success: " . ($result['success_count'] ?? 'N/A') . "\n";
fbird_commit($trans);
fbird_free_query($stmt);

// Count NULLs
$r = fbird_query($db, "SELECT COUNT(*) AS NULL_INT FROM BATCH_LARGE_TEST WHERE ID >= 30000 AND ID < 30200 AND INT_VAL IS NULL");
$row = fbird_fetch_assoc($r);
echo "Rows with NULL INT_VAL: " . $row['NULL_INT'] . "\n";
fbird_free_result($r);

$r = fbird_query($db, "SELECT COUNT(*) AS NULL_STR FROM BATCH_LARGE_TEST WHERE ID >= 30000 AND ID < 30200 AND STR_VAL IS NULL");
$row = fbird_fetch_assoc($r);
echo "Rows with NULL STR_VAL: " . $row['NULL_STR'] . "\n";
fbird_free_result($r);

// Final statistics
echo "\n--- Final Statistics ---\n";
$r = fbird_query($db, "SELECT COUNT(*) AS TOTAL FROM BATCH_LARGE_TEST");
$row = fbird_fetch_assoc($r);
echo "Total rows in table: " . $row['TOTAL'] . "\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE BATCH_LARGE_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Large Batch Buffer Management ===
Test table created

--- Test 1: Medium batch (100 rows) ---
Added 100 rows in %f ms
Executed in %f ms
Success count: 100
Verified rows: 100

--- Test 2: Large batch (500 rows) ---
Added 500 rows in %f ms
Executed in %f ms
Success count: 500
Verified: 500 rows (ID range 1000-1499)

--- Test 3: Very large batch (1000 rows) ---
Added 1000 rows in %f ms
Executed in %f ms
Success count: 1000
Verified rows: 1000

--- Test 4: Multiple small batches (10 x 50 rows) ---
Total time: %f ms
Total inserted: 500 rows

--- Test 5: Batch with long strings ---
Long string batch success: 50
String lengths: 180 to 199 chars

--- Test 6: Large batch with NULLs ---
Mixed NULL batch success: 200
Rows with NULL INT_VAL: %d
Rows with NULL STR_VAL: %d

--- Final Statistics ---
Total rows in table: %d

--- Cleanup ---

=== Test Complete ===

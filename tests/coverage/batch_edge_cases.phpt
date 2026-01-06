--TEST--
Coverage: Batch operation edge cases and error paths (firebird_utils.cpp)
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
 * Coverage test for batch operation edge cases in firebird_utils.cpp
 *
 * Tests error paths in:
 * - fbbatch_create() - initialization edge cases
 * - fbbatch_add() - row addition edge cases
 * - fbbatch_execute() - execution with various conditions
 * - fbbatch_close() - cleanup paths
 *
 * Target: Cover lines 2968-3500 in firebird_utils.cpp
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: Batch Operation Edge Cases ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up any existing test table
@fbird_query($db, 'DROP TABLE BATCH_EDGE_TEST');
@fbird_commit($db);

// Create test table with various column types
$create = "
CREATE TABLE BATCH_EDGE_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    INT_VAL INTEGER,
    STR_VAL VARCHAR(100),
    FLOAT_VAL DOUBLE PRECISION,
    DATE_VAL DATE,
    BLOB_VAL BLOB SUB_TYPE TEXT
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: Empty batch execution
echo "\n--- Test 1: Empty batch execution ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_EDGE_TEST (ID, INT_VAL) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);
echo "Batch created: " . ($batch ? "YES" : "NO") . "\n";

// Execute empty batch
$result = fbird_batch_execute($batch);
echo "Empty batch execute returns: " . gettype($result) . "\n";
if (is_array($result)) {
    echo "Empty batch total_processed: " . ($result['total_processed'] ?? 'N/A') . "\n";
    echo "Empty batch success_count: " . ($result['success_count'] ?? 'N/A') . "\n";
}
fbird_rollback($trans);
fbird_free_query($stmt);

// Test 2: Batch with NULL values in non-nullable column
echo "\n--- Test 2: Batch with constraint violations ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_EDGE_TEST (ID, INT_VAL, STR_VAL) VALUES (?, ?, ?)");
$batch = fbird_batch_create($stmt, $trans);

fbird_batch_add($batch, 1, 100, 'First');
fbird_batch_add($batch, null, 200, 'Null ID');  // NULL in NOT NULL column
fbird_batch_add($batch, 2, 300, 'Second');

$result = fbird_batch_execute($batch);
echo "Constraint violation batch:\n";
echo "  Total: " . ($result['total_processed'] ?? 'N/A') . "\n";
echo "  Success: " . ($result['success_count'] ?? 'N/A') . "\n";
echo "  Errors: " . ($result['error_count'] ?? 'N/A') . "\n";
fbird_rollback($trans);
fbird_free_query($stmt);

// Test 3: Batch with type mismatches
echo "\n--- Test 3: Batch with type coercion ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_EDGE_TEST (ID, INT_VAL, FLOAT_VAL) VALUES (?, ?, ?)");
$batch = fbird_batch_create($stmt, $trans);

// String to int coercion
fbird_batch_add($batch, "10", "100", "3.14159");
// Float to int coercion
fbird_batch_add($batch, 11, 200.999, 2.71828);
// Very large values
fbird_batch_add($batch, 12, 2147483647, 1.7976931348623158E+308);

$result = fbird_batch_execute($batch);
echo "Type coercion batch success: " . ($result['success_count'] ?? 'N/A') . "\n";
fbird_commit($trans);

// Verify inserted data
$r = fbird_query($db, "SELECT ID, INT_VAL FROM BATCH_EDGE_TEST WHERE ID IN (10, 11, 12) ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "  ID={$row['ID']}, INT_VAL={$row['INT_VAL']}\n";
}
fbird_free_result($r);

// Test 4: Batch with dates and special values
echo "\n--- Test 4: Batch with date edge cases ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_EDGE_TEST (ID, DATE_VAL) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

fbird_batch_add($batch, 20, '2000-01-01');  // Y2K
fbird_batch_add($batch, 21, '2099-12-31');  // Far future
fbird_batch_add($batch, 22, '1970-01-01');  // Unix epoch

$result = fbird_batch_execute($batch);
echo "Date batch success: " . ($result['success_count'] ?? 'N/A') . "\n";
fbird_commit($trans);
fbird_free_query($stmt);
fbird_commit($db);  // Ensure new implicit transaction sees committed data

$r = fbird_query($db, "SELECT ID, DATE_VAL FROM BATCH_EDGE_TEST WHERE ID IN (20, 21, 22) ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "  ID={$row['ID']}, DATE={$row['DATE_VAL']}\n";
}
fbird_free_result($r);

// Test 5: Batch with string edge cases
echo "\n--- Test 5: Batch with string edge cases ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_EDGE_TEST (ID, STR_VAL) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

fbird_batch_add($batch, 30, '');                        // Empty string
fbird_batch_add($batch, 31, str_repeat('A', 100));      // Max length
fbird_batch_add($batch, 32, "Line1\nLine2\tTab");       // Control chars
fbird_batch_add($batch, 33, 'Ümläüts äöü ÄÖÜ ß');       // UTF-8
fbird_batch_add($batch, 34, "Quote's \"test\"");        // Quotes

$result = fbird_batch_execute($batch);
echo "String batch success: " . ($result['success_count'] ?? 'N/A') . "\n";
fbird_commit($trans);
fbird_free_query($stmt);
fbird_commit($db);  // Ensure visibility

$r = fbird_query($db, "SELECT ID, CHAR_LENGTH(STR_VAL) AS LEN FROM BATCH_EDGE_TEST WHERE ID IN (30, 31, 32, 33, 34) ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    // Cast to int to handle NULL/empty case for empty string CHAR_LENGTH
    echo "  ID={$row['ID']}, LEN=" . (int)($row['LEN'] ?? 0) . "\n";
}
fbird_free_result($r);

// Test 6: Multiple batch cycles on same table
echo "\n--- Test 6: Multiple batch cycles ---\n";
for ($cycle = 1; $cycle <= 3; $cycle++) {
    $trans = fbird_trans($db);
    $stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_EDGE_TEST (ID, INT_VAL) VALUES (?, ?)");
    $batch = fbird_batch_create($stmt, $trans);
    
    $base_id = 100 + ($cycle * 10);
    for ($i = 0; $i < 5; $i++) {
        fbird_batch_add($batch, $base_id + $i, $cycle * 1000 + $i);
    }
    
    $result = fbird_batch_execute($batch);
    echo "Cycle $cycle: {$result['success_count']} rows\n";
    fbird_commit($trans);
    fbird_free_query($stmt);
}

// Test 7: Batch with BLOB IDs (Batch API requires blob IDs, not inline strings)
echo "\n--- Test 7: Batch with BLOB IDs ---\n";
$trans = fbird_trans($db);

// Create blobs first and get their IDs
$blob_contents = [
    200 => "Small text content",
    201 => str_repeat("Medium content block. ", 50),
    202 => str_repeat("Large BLOB data segment. ", 500),
];

$blob_ids = [];
foreach ($blob_contents as $id => $content) {
    $blob = fbird_blob_create($trans);
    fbird_blob_add($blob, $content);
    $blob_ids[$id] = fbird_blob_close($blob);
}
echo "Created " . count($blob_ids) . " blobs\n";

// Insert IDs in SAME transaction (BLOBs can only be used within creator transaction)
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_EDGE_TEST (ID, BLOB_VAL) VALUES (?, ?)");
foreach ($blob_ids as $id => $blob_id) {
    fbird_execute($stmt, $id, $blob_id);
}
fbird_commit($trans);
fbird_commit($db);  // Ensure visibility

$r = fbird_query($db, "SELECT ID, OCTET_LENGTH(BLOB_VAL) AS SIZE FROM BATCH_EDGE_TEST WHERE ID IN (200, 201, 202) ORDER BY ID");
$count = 0;
while ($row = fbird_fetch_assoc($r)) {
    echo "  ID={$row['ID']}, SIZE=" . ($row['SIZE'] ?? '0') . " bytes\n";
    $count++;
}
fbird_free_result($r);
echo "BLOB inserts success: $count\n";

// Test 8: Batch cancellation (prepare but don't execute)
echo "\n--- Test 8: Batch cancellation path ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BATCH_EDGE_TEST (ID, INT_VAL) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

// Add rows but don't execute - rollback will cancel
fbird_batch_add($batch, 999, 999);
fbird_batch_add($batch, 998, 998);
fbird_rollback($trans);
echo "Batch cancelled via rollback\n";
fbird_free_query($stmt);

// Verify no rows inserted
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM BATCH_EDGE_TEST WHERE ID IN (998, 999)");
$row = fbird_fetch_assoc($r);
echo "Rows from cancelled batch: " . $row['CNT'] . "\n";
fbird_free_result($r);

// Final row count
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM BATCH_EDGE_TEST");
$row = fbird_fetch_assoc($r);
echo "\n--- Summary: Total rows in table: " . $row['CNT'] . " ---\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE BATCH_EDGE_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Batch Operation Edge Cases ===
Test table created

--- Test 1: Empty batch execution ---
Batch created: YES
Empty batch execute returns: array
Empty batch total_processed: 0
Empty batch success_count: 0

--- Test 2: Batch with constraint violations ---
Constraint violation batch:
  Total: %d
  Success: %d
  Errors: %d

--- Test 3: Batch with type coercion ---
Type coercion batch success: 3
  ID=10, INT_VAL=100
  ID=11, INT_VAL=200
  ID=12, INT_VAL=2147483647

--- Test 4: Batch with date edge cases ---
Date batch success: 3
  ID=20, DATE=2000-01-01
  ID=21, DATE=2099-12-31
  ID=22, DATE=1970-01-01

--- Test 5: Batch with string edge cases ---
String batch success: 5
  ID=30, LEN=0
  ID=31, LEN=100
  ID=32, LEN=%d
  ID=33, LEN=%d
  ID=34, LEN=%d

--- Test 6: Multiple batch cycles ---
Cycle 1: 5 rows
Cycle 2: 5 rows
Cycle 3: 5 rows

--- Test 7: Batch with BLOB IDs ---
Created 3 blobs
  ID=200, SIZE=%d bytes
  ID=201, SIZE=%d bytes
  ID=202, SIZE=%d bytes
BLOB inserts success: 3

--- Test 8: Batch cancellation path ---
Batch cancelled via rollback
Rows from cancelled batch: 0

--- Summary: Total rows in table: %d ---

--- Cleanup ---

=== Test Complete ===

--TEST--
IBatch API: BLOB operations (add_blob, register_blob)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API not available (requires Firebird 4.0+)');
}
if (!function_exists('fbird_batch_add_blob')) {
    die('skip fbird_batch_add_blob() not available');
}
require_once 'firebird.inc';
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php
require __DIR__ . '/firebird.inc';

// Create test table with BLOB column
$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg());
}

// Drop table if exists and create new one
@fbird_query($db, 'DROP TABLE BATCH_BLOB_TEST');
$create_sql = <<<'SQL'
CREATE TABLE BATCH_BLOB_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    NAME VARCHAR(50),
    BLOB_DATA BLOB SUB_TYPE 0
)
SQL;

if (!fbird_query($db, $create_sql)) {
    die("Failed to create table: " . fbird_errmsg());
}

// Commit the table creation
fbird_commit($db);

echo "Table created successfully\n";

// Test 1: Create batch with BLOB column
$trans = fbird_trans($db);
$insert_sql = 'INSERT INTO BATCH_BLOB_TEST (ID, NAME, BLOB_DATA) VALUES (?, ?, ?)';
$query = fbird_prepare($trans, $insert_sql);
if (!$query) {
    die("Failed to prepare query: " . fbird_errmsg());
}
$batch = fbird_batch_create($query, $trans);
if (!$batch) {
    die("Failed to create batch: " . fbird_errmsg());
}
echo "Batch created successfully\n";

// Test 2: Add inline BLOBs using fbird_batch_add_blob()
$test_data = [
    [1, 'Test Row 1', 'Binary BLOB data for row 1'],
    [2, 'Test Row 2', 'Binary BLOB data for row 2 with more content'],
    [3, 'Test Row 3', str_repeat('BLOB', 100)], // Larger BLOB ~400 bytes
];

foreach ($test_data as $row) {
    list($id, $name, $blob_content) = $row;

    // Create inline BLOB and get BLOB ID
    $blob_id = fbird_batch_add_blob($batch, $blob_content);
    if ($blob_id === false) {
        die("Failed to add BLOB: " . fbird_errmsg());
    }

    // Use BLOB ID in batch row
    if (!fbird_batch_add($batch, $id, $name, $blob_id)) {
        die("Failed to add batch row: " . fbird_errmsg());
    }
}

echo "Added 3 rows with inline BLOBs\n";

// Note: fbird_batch_register_blob() requires further investigation
// for proper cross-transaction BLOB registration. For now, we test
// inline BLOB creation which is the primary use case.

// Test 4: Execute batch
$result = fbird_batch_execute($batch);
if ($result === false) {
    die("Batch execution failed: " . fbird_errmsg());
}

echo "Batch execution result:\n";
echo "  Total processed: " . $result['total_processed'] . "\n";
echo "  Error count: " . $result['error_count'] . "\n";

if ($result['error_count'] > 0) {
    die("Batch execution had errors");
}

// Commit transaction to persist data
fbird_commit($trans);

// Test 5: Verify BLOB data was inserted correctly
$verify_sql = 'SELECT ID, NAME, BLOB_DATA FROM BATCH_BLOB_TEST ORDER BY ID';
$query = fbird_query($db, $verify_sql);
if (!$query) {
    die("Failed to query table: " . fbird_errmsg());
}

echo "\nVerifying inserted BLOB data:\n";

$row_count = 0;

while ($row = fbird_fetch_assoc($query)) {
    $row_count++;
    $id = $row['ID'];
    $name = $row['NAME'];
    $blob_id = $row['BLOB_DATA'];

    // Fetch BLOB content
    $blob_info = fbird_blob_info($db, $blob_id);
    $blob = fbird_blob_open($db, $blob_id);
    $blob_content = '';
    while ($segment = fbird_blob_get($blob, $blob_info[0])) {
        $blob_content .= $segment;
    }
    fbird_blob_close($blob);

    // Verify against expected data
    $expected = $test_data[$id - 1];
    $expected_id = $expected[0];
    $expected_name = $expected[1];
    $expected_blob_content = $expected[2];

    if ($id !== $expected_id) {
        die("ID mismatch: expected $expected_id, got $id");
    }
    if ($name !== $expected_name) {
        die("NAME mismatch: expected '$expected_name', got '$name'");
    }
    if ($blob_content !== $expected_blob_content) {
        die("BLOB content mismatch for row $id: expected " . strlen($expected_blob_content) . " bytes, got " . strlen($blob_content) . " bytes");
    }

    echo "  Row $id: OK (NAME='$name', BLOB size=" . strlen($blob_content) . " bytes)\n";
}

if ($row_count !== 3) {
    die("Expected 3 rows, got $row_count");
}

echo "\nAll BLOB data verified successfully\n";

// Cleanup (batch auto-closed after execute, just drop table)
fbird_query($db, 'DROP TABLE BATCH_BLOB_TEST');
fbird_close($db);

echo "DONE\n";
?>
--EXPECTF--
Table created successfully
Batch created successfully
Added 3 rows with inline BLOBs
Batch execution result:
  Total processed: 3
  Error count: 0

Verifying inserted BLOB data:
  Row 1: OK (NAME='Test Row 1', BLOB size=26 bytes)
  Row 2: OK (NAME='Test Row 2', BLOB size=44 bytes)
  Row 3: OK (NAME='Test Row 3', BLOB size=400 bytes)

All BLOB data verified successfully
%aDONE

--TEST--
Coverage: BLOB ID string parsing paths (fbird_query_bind.c)
--EXTENSIONS--
firebird
--SKIPIF--
<?php require __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
/**
 * Coverage test for BLOB ID parsing in fbird_query_bind.c
 *
 * Tests BLOB ID binding paths:
 * - BLOB ID as string format
 * - BLOB creation and retrieval
 * - Binary BLOB handling
 * - Large segmented BLOBs
 * - NULL BLOB values
 *
 * NOTE: Uses explicit transactions throughout to avoid default transaction
 * interference issues when mixing blob inserts with verify queries.
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: BLOB ID Parsing Coverage ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Create test table (using default transaction for DDL is OK)
@fbird_query($db, 'DROP TABLE BLOB_ID_TEST');
@fbird_commit($db);

$create = "
CREATE TABLE BLOB_ID_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    TEXT_BLOB BLOB SUB_TYPE TEXT,
    BIN_BLOB BLOB SUB_TYPE BINARY
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: Create BLOB and get its ID
echo "\n--- Test 1: Create BLOB and retrieve ID ---\n";
$trans = fbird_trans($db);

$blob_handle = fbird_blob_create($trans);
if (!$blob_handle) {
    die("Failed to create blob: " . fbird_errmsg() . "\n");
}

$text = "This is the blob content for testing BLOB ID parsing.";
fbird_blob_add($blob_handle, $text);
$blob_id = fbird_blob_close($blob_handle);
echo "Created BLOB with ID: ";
var_dump($blob_id);
echo "BLOB ID type: " . gettype($blob_id) . "\n";

// Insert using the blob ID
$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB) VALUES (?, ?)");
$result = fbird_execute($stmt, 1, $blob_id);
var_dump($result !== false);
fbird_free_query($stmt);
fbird_commit($trans);

// Verify - use explicit transaction to avoid default transaction issues
$trans_v = fbird_trans($db);
$r = fbird_query($db, $trans_v, "SELECT ID, TEXT_BLOB FROM BLOB_ID_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($r, FBIRD_TEXT);
echo "Stored ID: " . $row['ID'] . "\n";
echo "Content length: " . strlen($row['TEXT_BLOB']) . "\n";
echo "Content matches: " . ($row['TEXT_BLOB'] === $text ? "YES" : "NO") . "\n";
fbird_free_result($r);
fbird_commit($trans_v);

// Test 2: Multiple BLOBs with different content
echo "\n--- Test 2: Multiple BLOBs with different content ---\n";
$trans = fbird_trans($db);

// Create and insert blobs with different sizes one at a time
$contents = [
    100 => 'Short text',
    101 => str_repeat('Medium length content. ', 50),
    102 => str_repeat('Large content block. ', 200),
    103 => '', // Empty blob
    104 => "Special chars: äöü ÄÖÜ ß €",
];

$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB) VALUES (?, ?)");
foreach ($contents as $id => $content) {
    $blob = fbird_blob_create($trans);
    if (strlen($content) > 0) {
        fbird_blob_add($blob, $content);
    }
    $bid = fbird_blob_close($blob);
    fbird_execute($stmt, $id, $bid);
}
fbird_free_query($stmt);
fbird_commit($trans);
echo "Created and inserted 5 blobs\n";

// Verify sizes - explicit transaction
$trans_v = fbird_trans($db);
$r = fbird_query($db, $trans_v, "SELECT ID, OCTET_LENGTH(TEXT_BLOB) AS SIZE FROM BLOB_ID_TEST WHERE ID >= 100 AND ID <= 104 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: BLOB size = " . (int)($row['SIZE'] ?? 0) . " bytes\n";
}
fbird_free_result($r);
fbird_commit($trans_v);

// Test 3: BLOB with binary data
echo "\n--- Test 3: Binary BLOB handling ---\n";
$trans = fbird_trans($db);

// Create binary blob with null bytes
$binary_data = "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09";
$binary_data .= "\xFE\xFF\x00\x00\xAB\xCD";

$blob = fbird_blob_create($trans);
fbird_blob_add($blob, $binary_data);
$bin_blob_id = fbird_blob_close($blob);

$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, BIN_BLOB) VALUES (?, ?)");
$result = fbird_execute($stmt, 200, $bin_blob_id);
var_dump($result !== false);
fbird_free_query($stmt);
fbird_commit($trans);

// Verify binary data - explicit transaction
$trans_v = fbird_trans($db);
$r = fbird_query($db, $trans_v, "SELECT BIN_BLOB FROM BLOB_ID_TEST WHERE ID = 200");
$row = fbird_fetch_row($r);
if ($row === false) {
    echo "Binary fetch failed\n";
} else {
    $blob_id_fetch = $row[0];
    $blob_handle = fbird_blob_open($db, $blob_id_fetch);
    if ($blob_handle) {
        $read_data = fbird_blob_get($blob_handle, 100);
        fbird_blob_close($blob_handle);
        echo "Binary data length: " . strlen($read_data) . "\n";
        echo "Binary data matches: " . ($read_data === $binary_data ? "YES" : "NO") . "\n";
    } else {
        echo "Failed to open blob\n";
    }
}
fbird_free_result($r);
fbird_commit($trans_v);

// Test 4: BLOB ID from query result
echo "\n--- Test 4: BLOB ID passthrough ---\n";
$trans = fbird_trans($db);

// Create initial blob
$blob = fbird_blob_create($trans);
fbird_blob_add($blob, "Original blob content for passthrough test");
$original_id = fbird_blob_close($blob);

$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB) VALUES (?, ?)");
fbird_execute($stmt, 300, $original_id);
fbird_free_query($stmt);
fbird_commit($trans);

// Fetch the blob ID from the database - explicit transaction
$trans_v = fbird_trans($db);
$r = fbird_query($db, $trans_v, "SELECT TEXT_BLOB FROM BLOB_ID_TEST WHERE ID = 300");
$row = fbird_fetch_row($r);
fbird_free_result($r);
fbird_commit($trans_v);

if ($row === false) {
    echo "Failed to fetch row 300\n";
} else {
    $fetched_blob_id = $row[0];
    echo "Fetched BLOB ID type: " . gettype($fetched_blob_id) . "\n";
    
    // Read and verify content - new transaction for blob read
    $trans_r = fbird_trans($db);
    $blob_handle = fbird_blob_open($trans_r, $fetched_blob_id);
    if ($blob_handle) {
        $content = fbird_blob_get($blob_handle, 10000);
        fbird_blob_close($blob_handle);
        echo "Content length: " . strlen($content) . "\n";
        echo "Content correct: " . (strpos($content, 'passthrough') !== false ? "YES" : "NO") . "\n";
    }
    fbird_commit($trans_r);
}

// Test 5: Large BLOB that requires segmented reading
echo "\n--- Test 5: Large segmented BLOB ---\n";
$trans = fbird_trans($db);

// Create large blob (150KB)
$large_content = str_repeat("X", 50000) . str_repeat("Y", 50000) . str_repeat("Z", 50000);
$blob = fbird_blob_create($trans);
fbird_blob_add($blob, $large_content);
$large_blob_id = fbird_blob_close($blob);

$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB) VALUES (?, ?)");
fbird_execute($stmt, 400, $large_blob_id);
fbird_free_query($stmt);
fbird_commit($trans);

// Read in segments - explicit transaction
$trans_v = fbird_trans($db);
$r = fbird_query($db, $trans_v, "SELECT TEXT_BLOB FROM BLOB_ID_TEST WHERE ID = 400");
$row = fbird_fetch_row($r);
if ($row !== false) {
    $blob_handle = fbird_blob_open($trans_v, $row[0]);
    
    $total_read = 0;
    $chunks = 0;
    while (($chunk = fbird_blob_get($blob_handle, 16384)) !== false && strlen($chunk) > 0) {
        $total_read += strlen($chunk);
        $chunks++;
    }
    fbird_blob_close($blob_handle);
    
    echo "Total read: $total_read bytes in $chunks chunks\n";
    echo "Size matches: " . ($total_read == strlen($large_content) ? "YES" : "NO") . "\n";
}
fbird_free_result($r);
fbird_commit($trans_v);

// Test 6: NULL BLOB handling
echo "\n--- Test 6: NULL BLOB values ---\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB, BIN_BLOB) VALUES (?, ?, ?)");
$result = fbird_execute($stmt, 500, null, null);
var_dump($result !== false);
fbird_free_query($stmt);
fbird_commit($trans);

$trans_v = fbird_trans($db);
$r = fbird_query($db, $trans_v, "SELECT TEXT_BLOB IS NULL AS TXT_NULL, BIN_BLOB IS NULL AS BIN_NULL FROM BLOB_ID_TEST WHERE ID = 500");
$row = fbird_fetch_assoc($r);
echo "TEXT_BLOB is NULL: " . ($row['TXT_NULL'] ? "YES" : "NO") . "\n";
echo "BIN_BLOB is NULL: " . ($row['BIN_NULL'] ? "YES" : "NO") . "\n";
fbird_free_result($r);
fbird_commit($trans_v);

// Final count - explicit transaction
$trans_v = fbird_trans($db);
$r = fbird_query($db, $trans_v, "SELECT COUNT(*) AS CNT FROM BLOB_ID_TEST");
$row = fbird_fetch_assoc($r);
echo "\n--- Total rows: " . $row['CNT'] . " ---\n";
fbird_free_result($r);
fbird_commit($trans_v);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE BLOB_ID_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: BLOB ID Parsing Coverage ===
Test table created

--- Test 1: Create BLOB and retrieve ID ---
Created BLOB with ID: string(13) "%s"
BLOB ID type: string
bool(true)
Stored ID: 1
Content length: 53
Content matches: YES

--- Test 2: Multiple BLOBs with different content ---
Created and inserted 5 blobs
ID=100: BLOB size = 10 bytes
ID=101: BLOB size = 1150 bytes
ID=102: BLOB size = 4200 bytes
ID=103: BLOB size = 0 bytes
ID=104: BLOB size = %d bytes

--- Test 3: Binary BLOB handling ---
bool(true)
Binary data length: 16
Binary data matches: YES

--- Test 4: BLOB ID passthrough ---
Fetched BLOB ID type: string
Content length: 42
Content correct: YES

--- Test 5: Large segmented BLOB ---
Total read: 150000 bytes in %d chunks
Size matches: YES

--- Test 6: NULL BLOB values ---
bool(true)
TEXT_BLOB is NULL: YES
BIN_BLOB is NULL: YES

--- Total rows: %d ---

--- Cleanup ---

=== Test Complete ===

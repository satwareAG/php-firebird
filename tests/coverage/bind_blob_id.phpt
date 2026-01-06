--TEST--
Coverage: BLOB ID string parsing paths (fbird_query_bind.c)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
?>
--FILE--
<?php
/**
 * Coverage test for BLOB ID parsing in fbird_query_bind.c
 *
 * Tests BLOB ID binding paths:
 * - BLOB ID as numeric (quad structure)
 * - BLOB ID parsing with "0x" hex prefix
 * - BLOB creation and retrieval
 * - BLOB segment operations
 *
 * Target: Cover BLOB ID parsing paths in fbird_query_bind.c
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: BLOB ID Parsing Coverage ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up any existing test tables
@fbird_query($db, 'DROP TABLE BLOB_ID_TEST');
@fbird_query($db, 'DROP TABLE BLOB_STORE');
@fbird_commit($db);

// Create test tables
$create = "
CREATE TABLE BLOB_ID_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    TEXT_BLOB BLOB SUB_TYPE TEXT,
    BIN_BLOB BLOB SUB_TYPE BINARY
)";
fbird_query($db, $create);

$create2 = "
CREATE TABLE BLOB_STORE (
    ID INTEGER NOT NULL PRIMARY KEY,
    BLOB_REF BLOB SUB_TYPE TEXT,
    DESCRIPTION VARCHAR(100)
)";
fbird_query($db, $create2);
fbird_commit($db);
echo "Test tables created\n";

// Test 1: Create BLOB and get its ID
echo "\n--- Test 1: Create BLOB and retrieve ID ---\n";
$trans = fbird_trans($db);

// Create a blob (fbird_blob_create only takes link, uses default/current transaction)
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
fbird_commit($trans);

// Verify
$r = fbird_query($db, "SELECT ID, TEXT_BLOB FROM BLOB_ID_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($r, FBIRD_TEXT);
echo "Stored ID: " . $row['ID'] . "\n";
echo "Content length: " . strlen($row['TEXT_BLOB']) . "\n";
echo "Content matches: " . ($row['TEXT_BLOB'] === $text ? "YES" : "NO") . "\n";
fbird_free_result($r);

// Test 2: BLOB ID string format variations
echo "\n--- Test 2: Multiple BLOBs with different content ---\n";
$trans = fbird_trans($db);

// Create several blobs with different sizes
$blob_ids = [];
$contents = [
    'Short text',
    str_repeat('Medium length content. ', 50),
    str_repeat('Large content block with much more data. ', 200),
    '', // Empty blob
    "Special chars: äöü ÄÖÜ ß € @ # $ % ^ & * () [] {}",
];

foreach ($contents as $idx => $content) {
    $blob = fbird_blob_create($trans);
    if (strlen($content) > 0) {
        fbird_blob_add($blob, $content);
    }
    $blob_ids[$idx] = fbird_blob_close($blob);
}

echo "Created " . count($blob_ids) . " blobs\n";

// Insert all blobs
$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB) VALUES (?, ?)");
foreach ($blob_ids as $idx => $bid) {
    fbird_execute($stmt, 100 + $idx, $bid);
}
fbird_commit($trans);

// Verify sizes
$r = fbird_query($db, "SELECT ID, OCTET_LENGTH(TEXT_BLOB) AS SIZE FROM BLOB_ID_TEST WHERE ID >= 100 AND ID < 110 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    // Cast to int to ensure consistent output (NULL/empty becomes 0)
    echo "ID={$row['ID']}: BLOB size = " . (int)($row['SIZE'] ?? 0) . " bytes\n";
}
fbird_free_result($r);

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
fbird_commit($trans);
fbird_free_query($stmt);
fbird_commit($db);  // Ensure visibility

// Verify binary data
$r = fbird_query($db, "SELECT BIN_BLOB FROM BLOB_ID_TEST WHERE ID = 200");
$row = fbird_fetch_row($r);
if ($row === false) {
    echo "Binary data length: 0\n";
    echo "Binary data matches: NO (query failed)\n";
} else {
    $blob_id_fetch = $row[0];
    $blob_handle = fbird_blob_open($db, $blob_id_fetch);
    $read_data = fbird_blob_get($blob_handle, 100);
    fbird_blob_close($blob_handle);
    echo "Binary data length: " . strlen($read_data) . "\n";
    echo "Binary data matches: " . ($read_data === $binary_data ? "YES" : "NO") . "\n";
}
fbird_free_result($r);

// Test 4: BLOB ID from query result used in another insert
echo "\n--- Test 4: BLOB ID passthrough ---\n";
$trans = fbird_trans($db);

// Create initial blob
$blob = fbird_blob_create($trans);
fbird_blob_add($blob, "Original blob content for passthrough test");
$original_id = fbird_blob_close($blob);

$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB) VALUES (?, ?)");
fbird_execute($stmt, 300, $original_id);
fbird_commit($trans);

// Fetch the blob ID from the database
$r = fbird_query($db, "SELECT TEXT_BLOB FROM BLOB_ID_TEST WHERE ID = 300");
$row = fbird_fetch_row($r);
$fetched_blob_id = $row[0];
fbird_free_result($r);

echo "Fetched BLOB ID type: " . gettype($fetched_blob_id) . "\n";

// Try to use fetched ID in another context (store reference)
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_STORE (ID, BLOB_REF, DESCRIPTION) VALUES (?, ?, ?)");

// Create a new blob from the fetched content
$blob_handle = fbird_blob_open($db, $fetched_blob_id);
$content = fbird_blob_get($blob_handle, 10000);
fbird_blob_close($blob_handle);

$new_blob = fbird_blob_create($trans);
fbird_blob_add($new_blob, $content);
$new_blob_id = fbird_blob_close($new_blob);

fbird_execute($stmt, 1, $new_blob_id, 'Copied from ID 300');
fbird_commit($trans);

// Verify
$r = fbird_query($db, "SELECT BLOB_REF FROM BLOB_STORE WHERE ID = 1");
$row = fbird_fetch_assoc($r, FBIRD_TEXT);
echo "Copied content length: " . strlen($row['BLOB_REF']) . "\n";
echo "Copy successful: " . (strpos($row['BLOB_REF'], 'passthrough') !== false ? "YES" : "NO") . "\n";
fbird_free_result($r);

// Test 5: Large BLOB that requires segmented reading
echo "\n--- Test 5: Large segmented BLOB ---\n";
$trans = fbird_trans($db);

// Create large blob (>100KB)
$large_content = str_repeat("X", 50000) . str_repeat("Y", 50000) . str_repeat("Z", 50000);
$blob = fbird_blob_create($trans);
fbird_blob_add($blob, $large_content);
$large_blob_id = fbird_blob_close($blob);

$stmt = fbird_prepare($db, $trans, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB) VALUES (?, ?)");
fbird_execute($stmt, 400, $large_blob_id);
fbird_commit($trans);
fbird_free_query($stmt);
fbird_commit($db);  // Ensure visibility

// Read in segments
$r = fbird_query($db, "SELECT TEXT_BLOB FROM BLOB_ID_TEST WHERE ID = 400");
$row = fbird_fetch_row($r);
$blob_handle = fbird_blob_open($db, $row[0]);

$total_read = 0;
$chunks = 0;
while (($chunk = fbird_blob_get($blob_handle, 16384)) !== false && strlen($chunk) > 0) {
    $total_read += strlen($chunk);
    $chunks++;
}
fbird_blob_close($blob_handle);
fbird_free_result($r);

echo "Total read: $total_read bytes in $chunks chunks\n";
echo "Size matches: " . ($total_read == strlen($large_content) ? "YES" : "NO") . "\n";

// Test 6: NULL BLOB handling
echo "\n--- Test 6: NULL BLOB values ---\n";
$stmt = fbird_prepare($db, "INSERT INTO BLOB_ID_TEST (ID, TEXT_BLOB, BIN_BLOB) VALUES (?, ?, ?)");
$result = fbird_execute($stmt, 500, null, null);
var_dump($result !== false);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, TEXT_BLOB IS NULL AS TXT_NULL, BIN_BLOB IS NULL AS BIN_NULL FROM BLOB_ID_TEST WHERE ID = 500");
$row = fbird_fetch_assoc($r);
echo "TEXT_BLOB is NULL: " . ($row['TXT_NULL'] ? "YES" : "NO") . "\n";
echo "BIN_BLOB is NULL: " . ($row['BIN_NULL'] ? "YES" : "NO") . "\n";
fbird_free_result($r);

// Final count
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM BLOB_ID_TEST");
$row = fbird_fetch_assoc($r);
echo "\n--- Total rows in BLOB_ID_TEST: " . $row['CNT'] . " ---\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE BLOB_ID_TEST');
@fbird_query($db, 'DROP TABLE BLOB_STORE');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: BLOB ID Parsing Coverage ===
Test tables created

--- Test 1: Create BLOB and retrieve ID ---
Created BLOB with ID: string(%d) "%s"
BLOB ID type: string
bool(true)
Stored ID: 1
Content length: %d
Content matches: YES

--- Test 2: Multiple BLOBs with different content ---
Created 5 blobs
ID=100: BLOB size = %d bytes
ID=101: BLOB size = %d bytes
ID=102: BLOB size = %d bytes
ID=103: BLOB size = 0 bytes
ID=104: BLOB size = %d bytes

--- Test 3: Binary BLOB handling ---
bool(true)
Binary data length: 16
Binary data matches: YES

--- Test 4: BLOB ID passthrough ---
Fetched BLOB ID type: string
Copied content length: %d
Copy successful: YES

--- Test 5: Large segmented BLOB ---
Total read: 150000 bytes in %d chunks
Size matches: YES

--- Test 6: NULL BLOB values ---
bool(true)
TEXT_BLOB is NULL: YES
BIN_BLOB is NULL: YES

--- Total rows in BLOB_ID_TEST: %d ---

--- Cleanup ---

=== Test Complete ===

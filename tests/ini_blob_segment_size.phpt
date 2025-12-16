--TEST--
fbird.blob_segment_size INI setting
--EXTENSIONS--
firebird
--SKIPIF--
<?php require_once 'skipif.inc'; ?>
--FILE--
<?php
require_once 'firebird.inc';

echo "=== fbird.blob_segment_size INI test ===\n";

// Test 1: Check default value
echo "Test 1: Default value\n";
$default = ini_get('fbird.blob_segment_size');
var_dump($default);
var_dump($default === '4096');

// Test 2: Modify the setting
echo "\nTest 2: Modify to 8192\n";
ini_set('fbird.blob_segment_size', '8192');
var_dump(ini_get('fbird.blob_segment_size'));

// Test 3: Create and read blob with modified segment size
echo "\nTest 3: BLOB operations with segment size\n";
$db = init_db();

// Create test table
@fbird_query($db, "DROP TABLE blob_seg_test");
fbird_query($db, "CREATE TABLE blob_seg_test (id INTEGER NOT NULL PRIMARY KEY, data BLOB SUB_TYPE TEXT)");
fbird_commit($db);

// Create large blob data (larger than segment size)
$large_data = str_repeat("Test data for blob segment size testing. ", 500);
echo "Data size: " . strlen($large_data) . " bytes\n";

// Insert using blob stream
$blob = fbird_blob_create($db);
fbird_blob_add($blob, $large_data);
$blob_id = fbird_blob_close($blob);

$stmt = fbird_prepare($db, "INSERT INTO blob_seg_test (id, data) VALUES (?, ?)");
fbird_execute($stmt, [1, $blob_id]);
fbird_commit($db);
fbird_free_query($stmt);

// Read back
$result = fbird_query($db, "SELECT data FROM blob_seg_test WHERE id = 1");
$row = fbird_fetch_assoc($result, FBIRD_FETCH_BLOBS);
var_dump(strlen($row['DATA']) === strlen($large_data));
fbird_free_result($result);

// Clean up
@fbird_query($db, "DROP TABLE blob_seg_test");
fbird_commit($db);
fbird_close($db);

// Test 4: Restore default
echo "\nTest 4: Restore default\n";
ini_set('fbird.blob_segment_size', '4096');
var_dump(ini_get('fbird.blob_segment_size'));

echo "\nPASS\n";
?>
--EXPECT--
=== fbird.blob_segment_size INI test ===
Test 1: Default value
string(4) "4096"
bool(true)

Test 2: Modify to 8192
string(4) "8192"

Test 3: BLOB operations with segment size
Data size: 20000 bytes
bool(true)

Test 4: Restore default
string(4) "4096"

PASS

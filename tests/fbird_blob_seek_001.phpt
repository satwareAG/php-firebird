--TEST--
fbird_blob_seek() - BLOB seek operations
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);
if (!$db) {
    die("Failed to connect: " . fbird_errmsg());
}

// Create test table with stream BLOB (SUB_TYPE 1)
$trans_setup = fbird_trans($db);
@fbird_query($trans_setup, "DROP TABLE BLOB_SEEK_TEST");
@fbird_commit($trans_setup);

$trans_setup = fbird_trans($db);
fbird_query($trans_setup, "CREATE TABLE BLOB_SEEK_TEST (ID INTEGER PRIMARY KEY, DATA BLOB SUB_TYPE 1)");
fbird_commit($trans_setup);

// Test 1: Verify constants are defined
echo "=== Test 1: Constants defined ===\n";
var_dump(defined('FBIRD_BLOB_SEEK_SET'));
var_dump(defined('FBIRD_BLOB_SEEK_CUR'));
var_dump(defined('FBIRD_BLOB_SEEK_END'));
echo "SEEK_SET=" . FBIRD_BLOB_SEEK_SET . "\n";
echo "SEEK_CUR=" . FBIRD_BLOB_SEEK_CUR . "\n";
echo "SEEK_END=" . FBIRD_BLOB_SEEK_END . "\n";

// Test 2: Create and write a blob with known content
// Using fbird_blob_create_seekable() for stream blob that supports seeking
echo "\n=== Test 2: Create blob with known content ===\n";
$trans = fbird_trans($db);

// Use fbird_blob_create_seekable() - creates stream blob with seek support
$blob = fbird_blob_create_seekable($trans);
if (!$blob) {
    die("Failed to create blob: " . fbird_errmsg());
}

// Write content: "AAAAAAAAAA" (10 A's) + "BBBBBBBBBB" (10 B's) + "CCCCCCCCCC" (10 C's) = 30 bytes
fbird_blob_add($blob, str_repeat('A', 10));
fbird_blob_add($blob, str_repeat('B', 10));
fbird_blob_add($blob, str_repeat('C', 10));

$blob_id = fbird_blob_close($blob);
echo "Blob created, ID length: " . strlen($blob_id) . "\n";

// Insert into table
$stmt = fbird_prepare($trans, "INSERT INTO BLOB_SEEK_TEST (ID, DATA) VALUES (1, ?)");
fbird_execute($stmt, $blob_id);
fbird_free_query($stmt);
fbird_commit($trans);

// Test 3: Open blob and test seek with SEEK_SET
echo "\n=== Test 3: SEEK_SET (absolute positioning) ===\n";
$trans = fbird_trans($db);

// Fetch the blob
$result = fbird_query($trans, "SELECT DATA FROM BLOB_SEEK_TEST WHERE ID = 1");
$row = fbird_fetch_row($result);
$blob_id = $row[0];

// Open blob for reading using regular fbird_blob_open
$blob = fbird_blob_open_seekable($trans, $blob_id);
if (!$blob) {
    die("Failed to open blob: " . fbird_errmsg());
}

// Seek to position 10 (start of B's)
$new_pos = fbird_blob_seek($blob, 10, FBIRD_BLOB_SEEK_SET);
echo "Seek to 10 (SEEK_SET): new position = $new_pos\n";

// Read 5 bytes (should be "BBBBB")
$data = fbird_blob_get($blob, 5);
echo "Read 5 bytes: '$data'\n";

// Seek to position 20 (start of C's)
$new_pos = fbird_blob_seek($blob, 20, FBIRD_BLOB_SEEK_SET);
echo "Seek to 20 (SEEK_SET): new position = $new_pos\n";

// Read 5 bytes (should be "CCCCC")
$data = fbird_blob_get($blob, 5);
echo "Read 5 bytes: '$data'\n";

// Seek back to start
$new_pos = fbird_blob_seek($blob, 0, FBIRD_BLOB_SEEK_SET);
echo "Seek to 0 (SEEK_SET): new position = $new_pos\n";

// Read 5 bytes (should be "AAAAA")
$data = fbird_blob_get($blob, 5);
echo "Read 5 bytes: '$data'\n";

fbird_blob_close($blob);

// Test 4: SEEK_CUR (relative positioning)
echo "\n=== Test 4: SEEK_CUR (relative positioning) ===\n";

// Re-open blob
$blob = fbird_blob_open_seekable($trans, $blob_id);

// Read 5 bytes first (now at position 5)
$data = fbird_blob_get($blob, 5);
echo "Read 5 bytes (starts at 0): '$data'\n";

// Seek forward 10 bytes from current position (5 + 10 = 15)
$new_pos = fbird_blob_seek($blob, 10, FBIRD_BLOB_SEEK_CUR);
echo "Seek +10 (SEEK_CUR from 5): new position = $new_pos\n";

// Read 5 bytes (should be "BBBBB" - positions 15-19)
$data = fbird_blob_get($blob, 5);
echo "Read 5 bytes: '$data'\n";

fbird_blob_close($blob);

// Test 5: SEEK_END (from end)
echo "\n=== Test 5: SEEK_END (from end) ===\n";

// Re-open blob
$blob = fbird_blob_open_seekable($trans, $blob_id);

// Seek to 10 bytes before end (30 - 10 = 20, start of C's)
$new_pos = fbird_blob_seek($blob, -10, FBIRD_BLOB_SEEK_END);
echo "Seek -10 (SEEK_END): new position = $new_pos\n";

// Read 5 bytes (should be "CCCCC")
$data = fbird_blob_get($blob, 5);
echo "Read 5 bytes: '$data'\n";

fbird_blob_close($blob);

// Test 6: Error handling
echo "\n=== Test 6: Error handling ===\n";

// Test with invalid parameters
$blob = fbird_blob_open_seekable($trans, $blob_id);

// Invalid whence
$result = @fbird_blob_seek($blob, 0, 99);
if ($result === false) {
    echo "Invalid whence correctly returned false\n";
}

fbird_blob_close($blob);

fbird_commit($trans);

echo "\n=== Cleanup ===\n";
$trans_cleanup = fbird_trans($db);
fbird_query($trans_cleanup, "DROP TABLE BLOB_SEEK_TEST");
@fbird_commit($trans_cleanup);

fbird_close($db);
echo "Test completed successfully!\n";
?>
--EXPECT--
=== Test 1: Constants defined ===
bool(true)
bool(true)
bool(true)
SEEK_SET=0
SEEK_CUR=1
SEEK_END=2

=== Test 2: Create blob with known content ===
Blob created, ID length: 13

=== Test 3: SEEK_SET (absolute positioning) ===
Seek to 10 (SEEK_SET): new position = 10
Read 5 bytes: 'BBBBB'
Seek to 20 (SEEK_SET): new position = 20
Read 5 bytes: 'CCCCC'
Seek to 0 (SEEK_SET): new position = 0
Read 5 bytes: 'AAAAA'

=== Test 4: SEEK_CUR (relative positioning) ===
Read 5 bytes (starts at 0): 'AAAAA'
Seek +10 (SEEK_CUR from 5): new position = 15
Read 5 bytes: 'BBBBB'

=== Test 5: SEEK_END (from end) ===
Seek -10 (SEEK_END): new position = 20
Read 5 bytes: 'CCCCC'

=== Test 6: Error handling ===
Invalid whence correctly returned false

=== Cleanup ===
Test completed successfully!

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE BLOB_SEEK_TEST");
    @fbird_close($db);
}
?>

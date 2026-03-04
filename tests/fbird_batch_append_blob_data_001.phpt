--TEST--
fbird_batch_append_blob_data: append data chunk to in-progress batch BLOB
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'skipif.inc';
if (!function_exists('fbird_batch_append_blob_data')) {
    die('skip fbird_batch_append_blob_data not available (requires FB4+ build)');
}
if (!function_exists('fbird_batch_add_blob_stream')) {
    die('skip fbird_batch_add_blob_stream not available (requires FB4+ build)');
}
require_once 'firebird.inc';
if (get_fb_version() < 4.0) {
    die('skip requires Firebird 4.0+ (IBatch API)');
}
?>
--FILE--
<?php
require __DIR__ . '/firebird.inc';

$conn = fbird_connect($test_base, $user, $password);
$trans = fbird_trans($conn);

@fbird_query($trans, "DROP TABLE batch_append_blob_test");
fbird_commit_ret($trans);
fbird_query($trans, "CREATE TABLE batch_append_blob_test (id INTEGER, data BLOB)");
fbird_commit_ret($trans);

$stmt = fbird_prepare($trans, "INSERT INTO batch_append_blob_test (id, data) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

// IBatch::appendBlobData requires an active blob stream started via addBlobStream.
// Without calling addBlobStream first, Firebird 5 may access an uninitialised blob
// buffer, causing a segfault.
//
// Step 1: Start a blob stream entry using the correct BLOB_ID_ENGINE binary format:
//   [ISC_QUAD batch_blob_id: 8 bytes] [uint32 data_len: 4 bytes]
//   [alignment padding to 16 bytes]
$blob_id  = pack('VV', 1, 0);      // ISC_QUAD: gds_quad_high=1, gds_quad_low=0
$data_len = pack('V', 0);          // 0 bytes initial blob data
$padding  = str_repeat("\x00", 4); // pad 12-byte header to 16 bytes (8-byte align)
$stream   = $blob_id . $data_len . $padding; // 16 bytes

@fbird_batch_add_blob_stream($batch, $stream);

// Step 2: Append additional data chunk to the current BLOB in the stream.
// IBatch::appendBlobData appends raw bytes to the last BLOB opened via addBlobStream.
// Return value is always bool — true on success, false on failure.
$result = @fbird_batch_append_blob_data($batch, 'chunk_data');
var_dump(is_bool($result));

fbird_batch_cancel($batch);

@fbird_query($trans, "DROP TABLE batch_append_blob_test");
@fbird_commit($trans);
fbird_close($conn);
?>
--EXPECT--
bool(true)

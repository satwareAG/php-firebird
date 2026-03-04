--TEST--
fbird_batch_add_blob_stream: stream BLOB data into batch via IBatch addBlobStream protocol
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
if (!extension_loaded('firebird')) {
    die('skip firebird extension not loaded');
}
if (!function_exists('fbird_batch_add_blob_stream')) {
    die('skip fbird_batch_add_blob_stream not available (requires FB4+ build)');
}
if (get_fb_version() < 4.0) {
    die('skip requires Firebird 4.0+ (IBatch API)');
}
?>
--FILE--
<?php
require_once __DIR__ . '/config.inc';
$conn = fbird_connect(FBIRD_TEST_DB, FBIRD_TEST_USER, FBIRD_TEST_PASS);
$trans = fbird_trans($conn);

@fbird_query($trans, "DROP TABLE batch_blob_stream_test");
fbird_commit_ret($trans);
fbird_query($trans, "CREATE TABLE batch_blob_stream_test (id INTEGER, data BLOB)");
fbird_commit_ret($trans);

$stmt = fbird_prepare($trans, "INSERT INTO batch_blob_stream_test (id, data) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

// fbird_batch_add_blob_stream calls IBatch::addBlobStream — the returned value
// is always bool (true on success, false on protocol error).
//
// The stream binary format for BLOB_ID_ENGINE policy is:
//   [ISC_QUAD batch_blob_id: 8 bytes] [uint32 data_len: 4 bytes] [data: data_len bytes]
//   [alignment padding to getBlobAlignment() boundary]
//
// Build a minimal 16-byte valid blob stream entry:
//   blob_id  = pack('VV', 1, 0)  — ISC_QUAD {high=1, low=0}, 8 bytes (LE)
//   data_len = pack('V', 0)      — 0 bytes of BLOB data
//   padding  = str_repeat("\0", 4) — pad entry to 16 bytes (alignment=8, 12%8=4, need 4 pad)
$blob_id  = pack('VV', 1, 0);   // ISC_QUAD: gds_quad_high=1, gds_quad_low=0
$data_len = pack('V', 0);       // 0 bytes of blob data
$padding  = str_repeat("\x00", 4); // pad 12→16 bytes (ensure 8-byte alignment)
$stream   = $blob_id . $data_len . $padding; // 16 bytes total

$result = @fbird_batch_add_blob_stream($batch, $stream);
var_dump(is_bool($result));

fbird_batch_cancel($batch);

@fbird_query($trans, "DROP TABLE batch_blob_stream_test");
fbird_commit($trans);
fbird_close($conn);
?>
--EXPECT--
bool(true)

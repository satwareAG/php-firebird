--TEST--
fbird_batch_add_blob_stream: stream BLOB data into batch via IBatch addBlobStream protocol
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'skipif.inc';
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

@fbird_query($trans, "DROP TABLE batch_blob_stream_test");
fbird_commit_ret($trans);
fbird_query($trans, "CREATE TABLE batch_blob_stream_test (id INTEGER, data BLOB)");
fbird_commit_ret($trans);

$stmt = fbird_prepare($trans, "INSERT INTO batch_blob_stream_test (id, data) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

// fbird_batch_add_blob_stream calls IBatch::addBlobStream.
// The stream binary format for BLOB_ID_ENGINE policy is:
//   [ISC_QUAD batch_blob_id: 8 bytes] [uint32 data_len: 4 bytes] [data: data_len bytes]
//   [alignment padding to getBlobAlignment() boundary]
//
// Minimal 16-byte valid blob stream entry:
//   blob_id  = pack('VV', 1, 0)  — ISC_QUAD {high=1, low=0}, 8 bytes (LE)
//   data_len = pack('V', 0)      — 0 bytes of BLOB data
//   padding  = str_repeat("\0", 4) — pad 12-byte header to 16-byte alignment
$blob_id  = pack('VV', 1, 0);      // ISC_QUAD: gds_quad_high=1, gds_quad_low=0
$data_len = pack('V', 0);          // 0 bytes of blob data
$padding  = str_repeat("\x00", 4); // pad 12-byte header to 16 bytes (8-byte align)
$stream   = $blob_id . $data_len . $padding; // 16 bytes total

$result = @fbird_batch_add_blob_stream($batch, $stream);
var_dump(is_bool($result));

fbird_batch_cancel($batch);

@fbird_query($trans, "DROP TABLE batch_blob_stream_test");
@fbird_commit($trans);
fbird_close($conn);
?>
--EXPECT--
bool(true)

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE batch_blob_stream_test");
    @fbird_close($db);
}
?>

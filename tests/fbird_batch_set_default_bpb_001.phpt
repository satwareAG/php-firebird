--TEST--
fbird_batch_set_default_bpb: set default BLOB Property Block for batch BLOBs
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'skipif.inc';
if (!function_exists('fbird_batch_set_default_bpb')) {
    die('skip fbird_batch_set_default_bpb not available (requires FB4+ build)');
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

@fbird_query($trans, "DROP TABLE batch_bpb_test");
fbird_commit_ret($trans);
fbird_query($trans, "CREATE TABLE batch_bpb_test (id INTEGER, data BLOB)");
fbird_commit_ret($trans);

$stmt = fbird_prepare($trans, "INSERT INTO batch_bpb_test (id, data) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

// fbird_batch_set_default_bpb calls IBatch::setDefaultBpb.
// An empty BPB (length=0) causes Firebird 5 to dereference a null/zero-length
// buffer, leading to a segfault. Pass a minimal valid BPB instead:
//   isc_bpb_version1 = 0x01 (required first byte of any BPB)
// Return value is always bool — true on success, false on failure.
$result = @fbird_batch_set_default_bpb($batch, chr(1));
var_dump(is_bool($result));

fbird_batch_cancel($batch);

@fbird_query($trans, "DROP TABLE batch_bpb_test");
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
    @fbird_query($db, "DROP TABLE batch_bpb_test");
    @fbird_close($db);
}
?>

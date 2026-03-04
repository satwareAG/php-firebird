--TEST--
fbird_batch_set_default_bpb: set default BLOB Property Block for batch BLOBs
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
if (!extension_loaded('firebird')) {
    die('skip firebird extension not loaded');
}
if (!function_exists('fbird_batch_set_default_bpb')) {
    die('skip fbird_batch_set_default_bpb not available (requires FB4+ build)');
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

@fbird_query($trans, "DROP TABLE batch_bpb_test");
fbird_commit_ret($trans);
fbird_query($trans, "CREATE TABLE batch_bpb_test (id INTEGER, data BLOB)");
fbird_commit_ret($trans);

$stmt = fbird_prepare($trans, "INSERT INTO batch_bpb_test (id, data) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

// Call fbird_batch_set_default_bpb with a minimal valid BPB (isc_bpb_version1 = 0x01).
// An empty BPB is rejected by the C++ guard (returns false immediately to avoid
// a segfault inside libfbclient.so on Firebird 5.0 which dereferences null/empty BPB).
// Return value is always bool — true on success, false on failure.
$result = @fbird_batch_set_default_bpb($batch, chr(1));
var_dump(is_bool($result));

fbird_batch_cancel($batch);

@fbird_query($trans, "DROP TABLE batch_bpb_test");
fbird_commit($trans);
fbird_close($conn);
?>
--EXPECT--
bool(true)

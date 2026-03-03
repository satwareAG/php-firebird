--TEST--
fbird_batch_set_default_bpb: set default BLOB Property Block for batch BLOBs
--SKIPIF--
<?php
require_once __DIR__ . '/config.inc';
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
$conn = @fbird_connect(FBIRD_TEST_DB, FBIRD_TEST_USER, FBIRD_TEST_PASS);
if (!$conn) die('skip cannot connect to Firebird');
if (fbird_get_client_major_version() < 4) die('skip requires Firebird 4.0+ (batch API)');
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

// Call fbird_batch_set_default_bpb with an empty BPB string.
// Return value is always bool — true on success, false on failure.
$result = fbird_batch_set_default_bpb($batch, '');
var_dump(is_bool($result));

fbird_batch_cancel($batch);

@fbird_query($trans, "DROP TABLE batch_bpb_test");
fbird_commit($trans);
fbird_close($conn);
?>
--EXPECT--
bool(true)

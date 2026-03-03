--TEST--
fbird_batch_append_blob_data: append data chunk to in-progress batch BLOB
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

@fbird_query($trans, "DROP TABLE batch_append_blob_test");
fbird_commit_ret($trans);
fbird_query($trans, "CREATE TABLE batch_append_blob_test (id INTEGER, data BLOB)");
fbird_commit_ret($trans);

$stmt = fbird_prepare($trans, "INSERT INTO batch_append_blob_test (id, data) VALUES (?, ?)");
$batch = fbird_batch_create($stmt, $trans);

// Append data to batch BLOB — the function should succeed (return true or false,
// depending on whether a BLOB segment is currently open on this batch).
// The C++ implementation calls IBatch::appendBlobData which requires an active BLOB stream.
$result = fbird_batch_append_blob_data($batch, 'chunk_data');
var_dump(is_bool($result));

fbird_batch_cancel($batch);

@fbird_query($trans, "DROP TABLE batch_append_blob_test");
fbird_commit($trans);
fbird_close($conn);
?>
--EXPECT--
bool(true)

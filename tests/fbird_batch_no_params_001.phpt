--TEST--
fbird_batch_create() rejects statement with no input parameters (Issue #180)
--SKIPIF--
<?php
include("skipif.inc");
if (!function_exists('fbird_batch_create')) die("skip IBatch API not available (requires FB_API_VER >= 40)");
?>
--FILE--
<?php
require_once('config.inc');
require_once('common.inc');

$conn = fbird_connect($test_base, $user, $password, $charset);
$tx = fbird_trans($conn);

// Test 1: SELECT with no parameters must return false (not SIGFPE)
$stmt1 = fbird_prepare($conn, $tx, 'SELECT 1 FROM RDB$DATABASE');
$result1 = @fbird_batch_create($stmt1);
var_dump($result1);
echo "Error: " . fbird_errmsg() . "\n";
fbird_free_query($stmt1);

// Test 2: Parameterized INSERT must still work
fbird_query($conn, $tx, 'CREATE TABLE batch_test_180 (id INTEGER, name VARCHAR(50))');
fbird_commit_ret($tx);

// Test 2a: Non-parameterized DML must return false
$stmt2 = fbird_prepare($conn, $tx, 'DELETE FROM batch_test_180 WHERE 1=0');
$result2 = @fbird_batch_create($stmt2);
var_dump($result2);
fbird_free_query($stmt2);

// Test 3: Parameterized INSERT must work

$stmt3 = fbird_prepare($conn, $tx, 'INSERT INTO batch_test_180 (id, name) VALUES (?, ?)');
$batch = fbird_batch_create($stmt3);
var_dump(is_resource($batch) || is_object($batch));

// Clean up
fbird_batch_cancel($batch);
fbird_free_query($stmt3);
fbird_query($conn, $tx, 'DROP TABLE batch_test_180');
fbird_commit($tx);
fbird_close($conn);

echo "Done\n";
?>
--EXPECT--
bool(false)
Error: fbird_batch_create(): statement has no input parameters; batch operations require parameterized statements (e.g., INSERT ... VALUES (?, ?))
bool(false)
bool(true)
Done

--TEST--
fbird_execute() with PHP stream as BLOB parameter
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
require_once __DIR__ . '/config.inc';
$dsn = $host . ':/firebird/data/test.fdb';
$c = @fbird_connect($dsn, $user, $password);
if (!$c) die('skip cannot connect');
fbird_close($c);
?>
--FILE--
<?php
require_once __DIR__ . '/config.inc';

$dsn = $host . ':/firebird/data/test.fdb';
$conn = fbird_connect($dsn, $user, $password);

/* Create test table */
@fbird_query($conn, "DROP TABLE blob_stream_test");
fbird_query($conn, "CREATE TABLE blob_stream_test (id INTEGER, data BLOB SUB_TYPE TEXT)");
fbird_commit($conn);

/* Create a temp file with known content */
$tmpfile = tempnam(sys_get_temp_dir(), 'fb_blob_');
file_put_contents($tmpfile, "Hello from stream!\nLine 2 of blob data.");

/* Open as stream and bind to BLOB parameter */
$stream = fopen($tmpfile, 'rb');
$trans = fbird_trans_start($conn);
$stmt = fbird_prepare($trans, "INSERT INTO blob_stream_test (id, data) VALUES (1, ?)");
$result = fbird_execute($stmt, $stream);
fclose($stream);
fbird_free_query($stmt);
fbird_commit($trans);

/* Read back */
$result = fbird_query($conn, "SELECT data FROM blob_stream_test WHERE id = 1");
$row = fbird_fetch_assoc($result);
$blob_data = fbird_blob_info($conn, $row['DATA']);
echo "type: " . gettype($blob_data) . "\n";

/* Read the actual blob content */
$blob_handle = fbird_blob_open($conn, $row['DATA']);
$content = fbird_blob_get($blob_handle, 1000);
fbird_blob_close($blob_handle);
echo "content: " . $content . "\n";
fbird_free_result($result);

/* Cleanup */
@fbird_query($conn, "DROP TABLE blob_stream_test");
@fbird_commit($conn);
fbird_close($conn);
unlink($tmpfile);

echo "Done\n";
?>
--EXPECT--
type: array
content: Hello from stream!
Line 2 of blob data.
Done

--CLEAN--
<?php
require_once 'config.inc';
$db = @fbird_connect($host . ':/firebird/data/test.fdb', $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE blob_stream_test");
    @fbird_close($db);
}
?>

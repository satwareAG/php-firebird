--TEST--
Coverage: blob seek, segmented read/write, blob info, large blobs
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die("connect failed: " . fbird_errmsg());

// ---- Setup ----
fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BLOB_SEEK_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BLOB_SEEK_COV';
END");
fbird_commit($dbh);
fbird_query($dbh, 'CREATE TABLE BLOB_SEEK_COV (ID INTEGER NOT NULL PRIMARY KEY, BLB BLOB SUB_TYPE 0, TXT BLOB SUB_TYPE TEXT)');
fbird_commit($dbh);

// ---- Test 1: Write blob in stream mode, read back ----
echo "Test 1: stream blob write/read\n";
$blob = fbird_blob_create($dbh);
var_dump(is_resource($blob));
$data = str_repeat('ABCDEFGHIJ', 100); // 1000 bytes
fbird_blob_add($blob, $data);
$bid = fbird_blob_close($blob);
var_dump(is_string($bid));

fbird_query($dbh, 'INSERT INTO BLOB_SEEK_COV (ID, BLB) VALUES (1, ?)', $bid);
fbird_commit($dbh);

$res = fbird_query($dbh, 'SELECT BLB FROM BLOB_SEEK_COV WHERE ID = 1');
$row = fbird_fetch_row($res);
fbird_free_result($res);
$blob2 = fbird_blob_open($dbh, $row[0]);
var_dump(is_resource($blob2));
$content = fbird_blob_get($blob2, 1000);
var_dump(strlen($content) === 1000);
fbird_blob_close($blob2);

// ---- Test 2: fbird_blob_info ----
echo "Test 2: blob info\n";
$res = fbird_query($dbh, 'SELECT BLB FROM BLOB_SEEK_COV WHERE ID = 1');
$row = fbird_fetch_row($res);
fbird_free_result($res);
$info = fbird_blob_info($dbh, $row[0]);
var_dump(is_array($info));
var_dump(isset($info['length']));
var_dump((int)$info['length'] === 1000);

// ---- Test 3: Multiple blob_add calls (segmented write) ----
echo "Test 3: segmented write\n";
$blob = fbird_blob_create($dbh);
for ($i = 0; $i < 10; $i++) {
    fbird_blob_add($blob, "Segment-$i-" . str_repeat('X', 50));
}
$bid = fbird_blob_close($blob);
var_dump(is_string($bid));
fbird_query($dbh, 'INSERT INTO BLOB_SEEK_COV (ID, BLB) VALUES (2, ?)', $bid);
fbird_commit($dbh);

// Read back segmented blob
$res = fbird_query($dbh, 'SELECT BLB FROM BLOB_SEEK_COV WHERE ID = 2');
$row = fbird_fetch_row($res);
fbird_free_result($res);
$blob2 = fbird_blob_open($dbh, $row[0]);
$total = '';
while (($chunk = fbird_blob_get($blob2, 64)) !== false && $chunk !== '') {
    $total .= $chunk;
}
fbird_blob_close($blob2);
var_dump(strlen($total) > 0);

// ---- Test 4: Text blob (SUB_TYPE TEXT) ----
echo "Test 4: text blob\n";
$blob = fbird_blob_create($dbh);
$text = "Hello, Firebird!\nLine 2\nLine 3\n";
fbird_blob_add($blob, $text);
$bid = fbird_blob_close($blob);
fbird_query($dbh, 'INSERT INTO BLOB_SEEK_COV (ID, TXT) VALUES (3, ?)', $bid);
fbird_commit($dbh);

$res = fbird_query($dbh, 'SELECT TXT FROM BLOB_SEEK_COV WHERE ID = 3');
$row = fbird_fetch_row($res);
fbird_free_result($res);
$blob2 = fbird_blob_open($dbh, $row[0]);
$content = fbird_blob_get($blob2, 1024);
fbird_blob_close($blob2);
var_dump(strpos($content, 'Hello') !== false);

// ---- Test 5: fbird_blob_echo ----
echo "Test 5: blob echo\n";
$res = fbird_query($dbh, 'SELECT BLB FROM BLOB_SEEK_COV WHERE ID = 1');
$row = fbird_fetch_row($res);
fbird_free_result($res);
ob_start();
fbird_blob_echo($dbh, $row[0]);
$echoed = ob_get_clean();
var_dump(strlen($echoed) === 1000);

// ---- Test 6: Large blob (>64KB) ----
echo "Test 6: large blob\n";
$blob = fbird_blob_create($dbh);
$chunk = str_repeat('Z', 8192);
for ($i = 0; $i < 10; $i++) {
    fbird_blob_add($blob, $chunk);
}
$bid = fbird_blob_close($blob);
var_dump(is_string($bid));
fbird_query($dbh, 'INSERT INTO BLOB_SEEK_COV (ID, BLB) VALUES (4, ?)', $bid);
fbird_commit($dbh);

// Read back large blob to verify size
$res = fbird_query($dbh, 'SELECT BLB FROM BLOB_SEEK_COV WHERE ID = 4');
$row = fbird_fetch_row($res);
fbird_free_result($res);
$blob2 = fbird_blob_open($dbh, $row[0]);
$total = '';
while (($chunk2 = fbird_blob_get($blob2, 8192)) !== false && $chunk2 !== '') {
    $total .= $chunk2;
}
fbird_blob_close($blob2);
var_dump(strlen($total) === 81920);

// ---- Test 7: fbird_blob_import ----
echo "Test 7: blob import\n";
$tmpfile = tempnam(sys_get_temp_dir(), 'fbird_blob_');
file_put_contents($tmpfile, str_repeat('IMPORT', 200));
$fh = fopen($tmpfile, 'rb');
$bid = fbird_blob_import($dbh, $fh);
fclose($fh);
var_dump(is_string($bid));
fbird_query($dbh, 'INSERT INTO BLOB_SEEK_COV (ID, BLB) VALUES (5, ?)', $bid);
fbird_commit($dbh);

// Read back imported blob via fbird_blob_echo
$res = fbird_query($dbh, 'SELECT BLB FROM BLOB_SEEK_COV WHERE ID = 5');
$row = fbird_fetch_row($res);
fbird_free_result($res);
ob_start();
fbird_blob_echo($dbh, $row[0]);
$echoed = ob_get_clean();
var_dump(strlen($echoed) === 1200);
unlink($tmpfile);

// ---- Cleanup ----
@fbird_rollback($dbh);
@fbird_query($dbh, 'DROP TABLE BLOB_SEEK_COV');
@fbird_commit($dbh);
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
Test 1: stream blob write/read
bool(true)
bool(true)
bool(true)
bool(true)
Test 2: blob info
bool(true)
bool(true)
bool(true)
Test 3: segmented write
bool(true)
bool(true)
Test 4: text blob
bool(true)
Test 5: blob echo
bool(true)
Test 6: large blob
bool(true)
bool(true)
Test 7: blob import
bool(true)
bool(true)
Done

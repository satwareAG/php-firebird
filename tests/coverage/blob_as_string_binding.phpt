--TEST--
Coverage: BLOB parameter binding via string (lines 800-870 fbird_query_bind.c)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die("connect failed: " . fbird_errmsg());

// Setup table with BLOB column
fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BLOB_STR_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BLOB_STR_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, '
    CREATE TABLE BLOB_STR_COV (
        ID      INTEGER NOT NULL PRIMARY KEY,
        DATA    BLOB SUB_TYPE TEXT,
        BINDATA BLOB SUB_TYPE 0
    )');
fbird_commit($dbh);

$ins = fbird_prepare($dbh, 'INSERT INTO BLOB_STR_COV (ID, DATA, BINDATA) VALUES (?, ?, ?)');

// Test 1: Insert a plain string into BLOB TEXT column
// Triggers the "OO API blob create from string" path in fbird_query_bind.c
echo "Test 1: insert string into TEXT BLOB\n";
$r = fbird_execute($ins, 1, 'Hello, this is blob text content!', null);
var_dump($r !== false);

// Test 2: Insert a longer string
echo "Test 2: insert long string into TEXT BLOB\n";
$long = str_repeat('x', 10000);
$r = fbird_execute($ins, 2, $long, null);
var_dump($r !== false);

// Test 3: Insert binary string into BLOB BINARY column
echo "Test 3: insert binary data into BLOB\n";
$bin = "\x00\x01\x02\x03\xff\xfe\xfd";
$r = fbird_execute($ins, 3, null, $bin);
var_dump($r !== false);

// Test 4: Insert string with newlines and special chars
echo "Test 4: insert multiline string\n";
$multi = "Line 1\nLine 2\nLine 3\t<tab>";
$r = fbird_execute($ins, 4, $multi, null);
var_dump($r !== false);

// Test 5: Insert NULL blob
echo "Test 5: insert NULL blob\n";
$r = fbird_execute($ins, 5, null, null);
var_dump($r !== false);

// Test 6: Use blob handle from fbird_blob_create
echo "Test 6: insert via blob handle\n";
$blob_handle = fbird_blob_create($dbh);
fbird_blob_add($blob_handle, 'handle blob content');
$blob_id = fbird_blob_close($blob_handle);
// Insert using blob id string
$r = fbird_execute($ins, 6, null, $blob_id);
var_dump($r !== false);

fbird_commit($dbh);

// Verify row count
$q = fbird_query($dbh, 'SELECT COUNT(*) FROM BLOB_STR_COV');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 7: all rows inserted\n";
var_dump((int)$row[0] === 6);

// Read back string blob — TEXT blobs may return as string or blob-id object
$q = fbird_query($dbh, 'SELECT DATA FROM BLOB_STR_COV WHERE ID = 1');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 8: TEXT blob stored (non-null)\n";
var_dump($row[0] !== null && $row[0] !== false);

// Read back long blob
$q = fbird_query($dbh, 'SELECT DATA FROM BLOB_STR_COV WHERE ID = 2');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 9: long blob stored (non-null)\n";
var_dump($row[0] !== null && $row[0] !== false);

// Cleanup
@fbird_commit($dbh);
@fbird_query($dbh, 'DROP TABLE BLOB_STR_COV');
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECT--
Test 1: insert string into TEXT BLOB
bool(true)
Test 2: insert long string into TEXT BLOB
bool(true)
Test 3: insert binary data into BLOB
bool(true)
Test 4: insert multiline string
bool(true)
Test 5: insert NULL blob
bool(true)
Test 6: insert via blob handle
bool(true)
Test 7: all rows inserted
bool(true)
Test 8: TEXT blob stored (non-null)
bool(true)
Test 9: long blob stored (non-null)
bool(true)
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

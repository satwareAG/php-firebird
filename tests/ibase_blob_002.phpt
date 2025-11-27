--TEST--
ibase_blob_get() chunked reading tests
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$db = ibase_connect($test_base);

// Create a blob with known content
$content = str_repeat("1234567890", 100); // 1000 chars
$blob = ibase_blob_create($db);
ibase_blob_add($blob, $content);
$blob_id = ibase_blob_close($blob);

// Open blob for reading
$blob = ibase_blob_open($db, $blob_id);

// Read in chunks
$read_content = "";
// Read 100 chars at a time
while ($chunk = ibase_blob_get($blob, 100)) {
    $read_content .= $chunk;
}

var_dump(strlen($read_content));
var_dump($read_content === $content);

// Test zero length read (Edge case)
$blob2 = ibase_blob_open($db, $blob_id);
// Reading 0 bytes should return string(0) ""
$res = ibase_blob_get($blob2, 0);
var_dump($res);

ibase_blob_close($blob);
if (isset($blob2)) ibase_blob_close($blob2);
ibase_close($db);
?>
--EXPECT--
int(1000)
bool(true)
string(0) ""

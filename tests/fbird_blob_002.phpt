--TEST--
fbird_blob_get() chunked reading tests
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

// Create a blob with known content
$content = str_repeat("1234567890", 100); // 1000 chars
$blob = fbird_blob_create($db);
fbird_blob_add($blob, $content);
$blob_id = fbird_blob_close($blob);

// Open blob for reading
$blob = fbird_blob_open($db, $blob_id);

// Read in chunks
$read_content = "";
// Read 100 chars at a time
while ($chunk = fbird_blob_get($blob, 100)) {
    $read_content .= $chunk;
}

var_dump(strlen($read_content));
var_dump($read_content === $content);

// Test zero length read (Edge case)
$blob2 = fbird_blob_open($db, $blob_id);
// Reading 0 bytes should return string(0) ""
$res = fbird_blob_get($blob2, 0);
var_dump($res);

fbird_blob_close($blob);
if (isset($blob2)) fbird_blob_close($blob2);
fbird_close($db);
?>
--EXPECT--
int(1000)
bool(true)
string(0) ""

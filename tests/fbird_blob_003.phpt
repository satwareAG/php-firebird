--TEST--
fbird_blob_add() and fbird_blob_get() with >64KB data (segmentation test)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

// Create a blob with content larger than 64KB (USHRT_MAX) to trigger segmentation loop
// 700 x 100 chars = 70000 bytes
$part = str_repeat("0123456789", 10); // 100 chars
$content = str_repeat($part, 700); // 70000 chars

$blob = fbird_blob_create($db);
$res = fbird_blob_add($blob, $content);
var_dump($res);
$blob_id = fbird_blob_close($blob);
var_dump(is_string($blob_id));

// Open blob for reading
$blob = fbird_blob_open($db, $blob_id);

// Read in chunks larger than typical but smaller than total
// 8192 is standard buffer
$read_content = "";
while ($chunk = fbird_blob_get($blob, 8192)) {
    $read_content .= $chunk;
}

var_dump(strlen($read_content));
var_dump(md5($read_content) === md5($content));

fbird_blob_close($blob);
fbird_close($db);
?>
--EXPECT--
bool(true)
bool(true)
int(70000)
bool(true)

--TEST--
Firebird: coverage for blob_info, blob_echo, blob_import
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$db = fbird_connect($test_base);

// 1. Create a standard blob for testing info & echo
$bl_handle = fbird_blob_create($db);
$data = "Test Data For Blob Coverage";
fbird_blob_add($bl_handle, $data);
$blob_id = fbird_blob_close($bl_handle);

// 2. fbird_blob_info
echo "--- fbird_blob_info ---\n";
$info = fbird_blob_info($db, $blob_id);
var_dump($info['length'] === strlen($data));
var_dump($info['numseg'] > 0);
var_dump(isset($info['maxseg']));
var_dump(isset($info['stream']));
var_dump($info['isnull'] === false);

// 3. fbird_blob_echo
echo "--- fbird_blob_echo ---\n";
ob_start();
fbird_blob_echo($db, $blob_id);
$content = ob_get_clean();
var_dump($content === $data);

// 4. fbird_blob_import
echo "--- fbird_blob_import ---\n";
$tmpfile = tempnam(sys_get_temp_dir(), 'ib_test');
$import_data = "Imported Data From File Stream";
file_put_contents($tmpfile, $import_data);

$fp = fopen($tmpfile, 'r');
$import_id = fbird_blob_import($db, $fp);
fclose($fp);
unlink($tmpfile);

var_dump(is_string($import_id));

// Verify imported content
$open_h = fbird_blob_open($db, $import_id);
$fetched = fbird_blob_get($open_h, 1024);
fbird_blob_close($open_h);
var_dump($fetched === $import_data);

fbird_close($db);
?>
--EXPECT--
--- fbird_blob_info ---
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
--- fbird_blob_echo ---
bool(true)
--- fbird_blob_import ---
bool(true)
bool(true)

--TEST--
Bug: var_export returns NULL for valid resources
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$x = ibase_connect($test_base);
$trans = ibase_trans($x);
$query = ibase_prepare($trans, "SELECT 1 FROM RDB\$DATABASE");
$res = ibase_execute($query);

echo "Dump: ";
var_dump($res);

echo "Export: ";
var_export($res);
echo "\n";

ibase_free_result($res);
ibase_free_query($query);
ibase_commit($trans);
ibase_close($x);
?>
--EXPECTF--
Dump: resource(%d) of type (Firebird/InterBase query)
Export: NULL

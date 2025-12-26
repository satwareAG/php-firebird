--TEST--
Bug: var_export returns NULL for valid resources
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$x = fbird_connect($test_base);
$trans = fbird_trans($x);
$query = fbird_prepare($trans, "SELECT 1 FROM RDB\$DATABASE");
$res = fbird_execute($query);

echo "Dump: ";
var_dump($res);

echo "Export: ";
var_export($res);
echo "\n";

fbird_free_result($res);
fbird_free_query($query);
fbird_commit($trans);
fbird_close($x);
?>
--EXPECTF--
Dump: resource(%d) of type (Firebird query)
Export: NULL

--TEST--
fbird_num_params(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$x = fbird_connect($test_base);

$rs = fbird_prepare('SELECT * FROM test1 WHERE 1 = ? AND 2 = ?');
var_dump(fbird_num_params($rs));

?>
--EXPECTF--
int(2)

--TEST--
fbird_num_fields(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$x = fbird_connect($test_base);

var_dump(fbird_num_fields(fbird_query('SELECT * FROM test1')));

?>
--EXPECTF--
int(2)

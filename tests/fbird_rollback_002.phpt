--TEST--
fbird_rollback(): Make sure the method can be invoked with zero arguments
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

fbird_connect($test_base);

fbird_query('INSERT INTO test1 VALUES (100, 2)');

var_dump(fbird_rollback());

?>
--EXPECTF--
bool(true)

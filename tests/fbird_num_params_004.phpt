--TEST--
fbird_num_params(): Basic test
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

require("firebird.inc");

$x = fbird_connect($test_base);

$rs = fbird_prepare('SELECT * FROM test1 WHERE 1 = ? AND 2 = ?');
var_dump(fbird_num_params($rs));

$rs = fbird_prepare('SELECT * FROM test1 WHERE 1 = ? AND 2 = ? AND 3 = :x');
var_dump(fbird_num_params($rs));

?>
--EXPECTF--
int(2)

Warning: fbird_prepare(): Dynamic SQL Error SQL error code = -%d Column unknown X At line %d, column %d %s

Fatal error: Uncaught TypeError: fbird_num_params(): Argument #1 ($query) must be of type resource, %a
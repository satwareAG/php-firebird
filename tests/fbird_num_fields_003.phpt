--TEST--
fbird_num_fields(): Make sure passing an integer to the function throws an error.
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

var_dump(fbird_num_fields(1));

?>
--EXPECTF--
Warning: fbird_num_fields(): Argument #1 must be a Firebird query/result resource, int given in %s on line %d
bool(false)

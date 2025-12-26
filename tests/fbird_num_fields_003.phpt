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
Fatal error: Uncaught TypeError: fbird_num_fields(): Argument #1 ($query_result) must be of type resource, int given in %a

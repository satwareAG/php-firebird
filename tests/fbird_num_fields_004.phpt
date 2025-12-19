--TEST--
fbird_num_fields(): Make sure passing zero arguments to the function throws an error
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

var_dump(fbird_num_fields());

?>
--EXPECTF--
Fatal error: Uncaught ArgumentCountError: fbird_num_fields() expects exactly 1 argument, 0 given in %a

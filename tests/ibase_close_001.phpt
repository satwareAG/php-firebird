--TEST--
ibase_close(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$x = ibase_connect($test_base);
var_dump(ibase_close($x));
var_dump(ibase_close($x));
var_dump(ibase_close());

try {
    var_dump(ibase_close('foo'));
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}


?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
ibase_close(): Argument #1 ($link_identifier) must be of type resource, string given

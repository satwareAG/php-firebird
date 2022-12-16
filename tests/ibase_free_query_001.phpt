--TEST--
ibase_free_query(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$x = ibase_connect($test_base);

$q = ibase_prepare($x, 'SELECT 1 FROM test1 WHERE i = ?');
$q = ibase_prepare($x, 'SELECT 1 FROM test1 WHERE i = ?');
$q = ibase_prepare($x, 'SELECT 1 FROM test1 WHERE i = ?');

var_dump(ibase_free_query($q));
try {
    var_dump(ibase_free_query($q));
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}

try {
    var_dump(ibase_free_query($x));
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}


?>
--EXPECTF--
bool(true)
ibase_free_query(): supplied resource is not a valid Firebird/InterBase query resource
ibase_free_query(): supplied resource is not a valid Firebird/InterBase query resource

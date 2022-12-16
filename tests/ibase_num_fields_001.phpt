--TEST--
ibase_num_fields(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$x = ibase_connect($test_base);

var_dump(ibase_num_fields(ibase_query('SELECT * FROM test1')));

try {
    var_dump(ibase_num_fields(1));
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}

try {
    var_dump(ibase_num_fields());
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}
?>
--EXPECTF--
int(2)
ibase_num_fields(): Argument #1 ($query_result) must be of type resource, int given
ibase_num_fields() expects exactly 1 argument, 0 given

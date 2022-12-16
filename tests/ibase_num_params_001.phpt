--TEST--
ibase_num_params(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$x = ibase_connect($test_base);

$rs = ibase_prepare('SELECT * FROM test1 WHERE 1 = ? AND 2 = ?');
var_dump(ibase_num_params($rs));

try {
    $rs = ibase_prepare('SELECT * FROM test1 WHERE 1 = ? AND 2 = ?');
    var_dump(ibase_num_params());
} catch (ArgumentCountError $e) {
    echo $e->getMessage();
}


$rs = ibase_prepare('SELECT * FROM test1 WHERE 1 = ? AND 2 = ? AND 3 = :x');
try {
    var_dump(ibase_num_params($rs));
} catch (TypeError $e) {
    echo $e->getMessage();
}

?>
--EXPECTF--
int(2)
ibase_num_params() expects exactly 1 argument, 0 given
Warning: ibase_prepare(): Dynamic SQL Error SQL error code = -206 Column unknown X At line 1, column 51  in %s/ibase_num_params_001.php on line %d
ibase_num_params(): Argument #1 ($query) must be of type resource, bool given

--TEST--
fbird_rollback(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$x = fbird_connect($test_base);

$tr = fbird_trans($x);
fbird_query($tr, 'INSERT INTO test1 VALUES (100, 2)');
fbird_query($tr, 'INSERT INTO test1 VALUES (100, 2)');
fbird_query($tr, 'INSERT INTO test1 VALUES (100, 2)');

$rs = fbird_query($tr, 'SELECT COUNT(*) FROM test1 WHERE i = 100');
var_dump(fbird_fetch_row($rs));

var_dump(fbird_rollback($tr));

$rs = fbird_query('SELECT COUNT(*) FROM test1 WHERE i = 100');
var_dump(fbird_fetch_row($rs));

var_dump(fbird_rollback($x));
var_dump(fbird_rollback());

?>
--EXPECTF--
array(1) {
  [0]=>
  int(3)
}
bool(true)
array(1) {
  [0]=>
  int(0)
}
bool(true)
bool(true)

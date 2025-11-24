--TEST--
ibase_param_info(): Basic test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$x = ibase_connect($test_base);

$rs = ibase_prepare('SELECT * FROM test1 WHERE 1 = ? AND 2 = ?');
var_dump(ibase_param_info($rs, 1));

print "---\n";

var_dump(ibase_param_info($rs, 100));

?>
--EXPECTF--
array(10) {
  [0]=>
  string(%d) "%S"
  ["name"]=>
  string(%d) "%S"
  [1]=>
  string(%d) "%S"
  ["alias"]=>
  string(%d) "%S"
  [2]=>
  string(%d) "%S"
  ["relation"]=>
  string(%d) "%S"
  [3]=>
  string(1) "4"
  ["length"]=>
  string(1) "4"
  [4]=>
  string(7) "INTEGER"
  ["type"]=>
  string(7) "INTEGER"
}
---
bool(false)

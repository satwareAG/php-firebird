--TEST--
IBASE_UNIXTIME: return negative unix timestamp (old behaviour) for TIME fields
--SKIPIF--
<?php
include("skipif.inc");

// See also: tests/time_003.phpt
skip_if_ext_gte(61);

?>
--FILE--
<?php

require("interbase.inc");
require("common.inc");
ibase_connect($test_base);
test_time_unixtime();

?>
--EXPECTF--
array(3) {
  ["ID"]=>
  int(1)
  ["T1"]=>
  string(8) "15:45:59"
  ["T2"]=>
  string(19) "2025-11-06 15:45:59"
}
array(3) {
  ["ID"]=>
  int(1)
  ["T1"]=>
  int(-%d)
  ["T2"]=>
  int(1762436759)
}
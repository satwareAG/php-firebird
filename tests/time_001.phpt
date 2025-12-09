--TEST--
FBIRD_UNIXTIME: return negative unix timestamp (old behaviour) for TIME fields
--SKIPIF--
<?php
include("skipif.inc");

// See also: tests/time_003.phpt
skip_if_ext_gte(10);

?>
--FILE--
<?php

require("firebird.inc");
require("common.inc");
fbird_connect($test_base);
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

--TEST--
IBASE_UNIXTIME: ignore IBASE_UNIXTIME flag for TIME_TZ fields
--SKIPIF--
<?php
include("skipif.inc");
// See also: tests/time_002.phpt
skip_if_ext_lt(61);
skip_if_fb_lt(4);
skip_if_fbclient_lt(4);
?>
--FILE--
<?php

require("interbase.inc");
require("common.inc");
ibase_connect($test_base);
test_time_tz_unixtime();

?>
--EXPECTF--
array(3) {
  ["ID"]=>
  int(1)
  ["T1"]=>
  string(%d) "15:45:59 %s"
  ["T2"]=>
  string(%d) "2025-11-06 15:45:59 %s"
}
array(3) {
  ["ID"]=>
  int(1)
  ["T1"]=>
  string(%d) "15:45:59 %s"
  ["T2"]=>
  int(1762443959)
}

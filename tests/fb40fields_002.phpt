--TEST--
Test data for fields introduced in FB 4.0, with older client
--SKIPIF--
<?php
include("skipif.inc");
skip_if_fb_lt(4);
skip_if_fbclient_gte(4);
if (ibase_get_client_major_version() < 4) die("skip Firebird client library < 4.0 DOES NOT SUPPORT INT128 (mapping failed)");
?>
--FILE--
<?php

require("firebird.inc");
require("common.inc");
ibase_connect($test_base);
test_field_data40();

?>
--EXPECT--
array(8) {
  ["ID"]=>
  int(1)
  ["NUMERIC_4"]=>
  string(39) "3.1415926535897932384626433832795028841"
  ["DECIMAL_4"]=>
  string(39) "3.1415926535897932384626433832795028841"
  ["DECFLOAT_16"]=>
  string(17) "3.141592653589793"
  ["DECFLOAT_34"]=>
  string(35) "3.141592653589793238462643383279502"
  ["INT128_FIELD"]=>
  string(40) "-170141183460469231731687303715884105727"
  ["TIME_TZ"]=>
  string(8) "16:45:59"
  ["TIMESTAMP_TZ"]=>
  string(19) "2025-11-06 16:45:59"
}

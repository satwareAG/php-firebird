--TEST--
Test data for fields introduced in FB 4.0
--SKIPIF--
<?php
include("skipif.inc");
skip_if_fb_lt(4);
skip_if_fbclient_lt(4);
/* jane: PHP 8.2 crashes in method dispatch for DecFloat objects.
 * On PHP < 8.3, DECFLOAT returns strings (compile-time fallback). */
if (PHP_VERSION_ID < 80300) die('skip DecFloat objects require PHP 8.3+');
?>
--FILE--
<?php

require("firebird.inc");
require("common.inc");
fbird_connect($test_base);
test_field_data40();

?>
--EXPECTF--
array(8) {
  ["ID"]=>
  int(1)
  ["NUMERIC_4"]=>
  string(39) "3.1415926535897932384626433832795028841"
  ["DECIMAL_4"]=>
  string(39) "3.1415926535897932384626433832795028841"
  ["DECFLOAT_16"]=>
  object(Firebird\DecFloat)#%d (0) {
  }
  ["DECFLOAT_34"]=>
  object(Firebird\DecFloat)#%d (0) {
  }
  ["INT128_FIELD"]=>
  string(40) "-170141183460469231731687303715884105727"
  ["TIME_TZ"]=>
  string(22) "15:45:59 Europe/Berlin"
  ["TIMESTAMP_TZ"]=>
  string(33) "2025-11-06 15:45:59 Europe/Berlin"
}

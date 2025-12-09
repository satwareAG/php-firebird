--TEST--
ibase_field_info(): fields introduced in FB 4.0, with older client
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
test_fields40();

?>
--EXPECT--
ID/INTEGER/4
NUMERIC_4/VARCHAR/188
DECIMAL_4/VARCHAR/188
DECFLOAT_16/VARCHAR/92
DECFLOAT_34/VARCHAR/168
INT128_FIELD/VARCHAR/188
TIME_TZ/TIME/4
TIMESTAMP_TZ/TIMESTAMP/8

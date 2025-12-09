--TEST--
ibase_close(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also: tests/ibase_close_004.phpt
skip_if_ext_gte(61);

?>
--FILE--
<?php

require("firebird.inc");

$x = ibase_connect($test_base);
var_dump(ibase_close($x));
var_dump(ibase_close($x));
var_dump(ibase_close());

?>
--EXPECTF--
bool(true)
bool(true)
bool(true)

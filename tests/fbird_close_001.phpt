--TEST--
fbird_close(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also: tests/fbird_close_004.phpt
skip_if_ext_gte(61);

?>
--FILE--
<?php

require("firebird.inc");

$x = fbird_connect($test_base);
var_dump(fbird_close($x));
var_dump(fbird_close($x));
var_dump(fbird_close());

?>
--EXPECTF--
bool(true)
bool(true)
bool(true)

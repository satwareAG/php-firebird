--TEST--
ibase_close(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also: tests/ibase_close_001.phpt
skip_if_ext_lt(61);

?>
--FILE--
<?php

require("firebird.inc");

set_exception_handler("php_fbird_exception_handler");

$x = ibase_connect($test_base);
var_dump(ibase_close($x));
var_dump(ibase_close($x));
var_dump(ibase_close());

?>
--EXPECT--
bool(true)
bool(false)
bool(false)

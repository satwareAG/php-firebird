--TEST--
fbird_close(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also: tests/fbird_close_001.phpt
skip_if_ext_lt(10);

?>
--FILE--
<?php

require("firebird.inc");

set_exception_handler("php_fbird_exception_handler");

$x = fbird_connect($test_base);
var_dump(fbird_close($x));
var_dump(fbird_close($x));
var_dump(fbird_close());

?>
--EXPECT--
bool(true)
bool(false)
bool(false)

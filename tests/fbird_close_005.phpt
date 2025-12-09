--TEST--
ibase_close(): Make sure passing a string to the function throws an error.
--SKIPIF--
<?php

include("skipif.inc");

// See also: tests/ibase_close_003.phpt
skip_if_ext_lt(61);

?>
--FILE--
<?php

require("firebird.inc");

set_exception_handler("php_ibase_exception_handler");

$x = ibase_connect($test_base);
var_dump(ibase_close($x));
var_dump(ibase_close($x));
var_dump(ibase_close());
var_dump(ibase_close('foo'));

?>
--EXPECTF--
bool(true)
bool(false)
bool(false)
Fatal error: Uncaught TypeError: ibase_close(): Argument #1 ($link_identifier) must be of type resource or null, string given%A

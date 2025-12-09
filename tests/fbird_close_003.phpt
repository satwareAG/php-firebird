--TEST--
fbird_close(): Make sure passing a string to the function throws an error.
--SKIPIF--
<?php

include("skipif.inc");

// See also: tests/fbird_close_005.phpt
skip_if_ext_gte(61);
?>
--FILE--
<?php

require("firebird.inc");

$x = fbird_connect($test_base);
var_dump(fbird_close($x));
var_dump(fbird_close($x));
var_dump(fbird_close());
var_dump(fbird_close('foo'));

?>
--EXPECTF--
bool(true)
bool(true)
bool(true)

Fatal error: Uncaught TypeError: fbird_close(): Argument #1 ($link_identifier) must be of type resource, string given in %a

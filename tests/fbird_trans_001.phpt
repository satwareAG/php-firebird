--TEST--
ibase_trans(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also tests/ibase_trans_014.phpt
// See also tests/ibase_trans_015.phpt
skip_if_ext_gte(61);

?>
--FILE--
<?php

require("firebird.inc");

$x = ibase_connect($test_base);
var_dump(ibase_trans($x));
var_dump(ibase_trans(1));
var_dump(ibase_close());
var_dump(ibase_close($x));

?>
--EXPECTF--
resource(%d) of type (Firebird/InterBase transaction)
resource(%d) of type (Firebird/InterBase transaction)
bool(true)
bool(true)

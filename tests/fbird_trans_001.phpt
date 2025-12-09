--TEST--
fbird_trans(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also tests/fbird_trans_014.phpt
// See also tests/fbird_trans_015.phpt
skip_if_ext_gte(61);

?>
--FILE--
<?php

require("firebird.inc");

$x = fbird_connect($test_base);
var_dump(fbird_trans($x));
var_dump(fbird_trans(1));
var_dump(fbird_close());
var_dump(fbird_close($x));

?>
--EXPECTF--
resource(%d) of type (Firebird transaction)
resource(%d) of type (Firebird transaction)
bool(true)
bool(true)

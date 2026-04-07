--TEST--
fbird_trans(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also tests/fbird_trans_001.phpt
// See also tests/fbird_trans_015.phpt
skip_if_ext_lt(10);
skip_if_php_lt(8);

?>
--FILE--
<?php

require("firebird.inc");
require("common.inc");

set_exception_handler("php_fbird_exception_handler");

test_fbird_trans_014_015();

?>
--EXPECTF--
object(Firebird\Transaction)#%d (0) {
}
object(Firebird\Transaction)#%d (0) {
}
bool(true)
bool(false)

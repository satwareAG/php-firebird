--TEST--
ibase_trans(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also tests/ibase_trans_001.phpt
// See also tests/ibase_trans_015.phpt
skip_if_ext_lt(61);
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
resource(%d) of type (Firebird/InterBase transaction)
resource(%d) of type (Firebird/InterBase transaction)
bool(true)
bool(false)

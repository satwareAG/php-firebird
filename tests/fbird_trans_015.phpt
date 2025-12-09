--TEST--
fbird_trans(): Basic test
--SKIPIF--
<?php

include("skipif.inc");

// See also tests/fbird_trans_001.phpt
// See also tests/fbird_trans_014.phpt
skip_if_ext_lt(10);
skip_if_php_gte(8);

?>
--FILE--
<?php

require("firebird.inc");
require("common.inc");

test_fbird_trans_014_015();

?>
--EXPECTF--
resource(%d) of type (Firebird transaction)
resource(%d) of type (Firebird transaction)
bool(true)

Warning: fbird_close(): supplied resource is not a valid Firebird link resource%s
bool(false)

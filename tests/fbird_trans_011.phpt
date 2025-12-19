--TEST--
fbird_trans(): transaction control with SQL - commit explicitly
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");
fbird_connect($test_base);

(function() {
    var_dump($t = fbird_query("SET TRANSACTION"));
    var_dump(fbird_query($t, "COMMIT"));
    var_dump(fbird_query($t, "COMMIT"));
})();

?>
--EXPECTF--
resource(%d) of type (Firebird transaction)
bool(true)

Warning: fbird_query(): Dynamic SQL Error SQL error code = -901 invalid transaction handle (expecting explicit transaction start)%s
bool(false)

--TEST--
fbird_trans(): transaction control with SQL - commit default transaction
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");
fbird_connect($test_base);

(function() {
    var_dump(fbird_query("COMMIT"));
    var_dump(fbird_query("COMMIT"));
})();

?>
--EXPECT--
bool(true)
bool(true)

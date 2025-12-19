--TEST--
fbird_trans(): handles
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");
fbird_connect($test_base);

(function() {
    $t = fbird_query("SET TRANSACTION");
    var_dump($t);
})();

?>
--EXPECTF--
resource(%d) of type (Firebird transaction)

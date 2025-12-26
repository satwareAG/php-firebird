--TEST--
fbird_trans(): handles
--SKIPIF--
<?php
include("skipif.inc");
// On FB2.5 server "invalid transaction handle" happens on fetch.
// See also: tests/fbird_trans_006.phpt
// die("skip further investigation needed");
// skip_if_fb_gt(2.5);
?>
--FILE--
<?php

require("firebird.inc");
fbird_connect($test_base);

(function() {
    var_dump($t = fbird_query("SET TRANSACTION"));
    var_dump(fbird_rollback($t));
    var_dump($t);
    var_dump($q = fbird_query($t, "SELECT * FROM TEST1"));
    if ($q) {
	    var_dump(fbird_fetch_assoc($q));
    }
})();

?>
--EXPECTF--
resource(%d) of type (Firebird transaction)
bool(true)
resource(%d) of type (Firebird transaction)

Warning: fbird_query(): invalid transaction handle (expecting explicit transaction start) %s
bool(false)

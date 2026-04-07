--TEST--
fbird_trans(): handles
--SKIPIF--
<?php
include("skipif.inc");
// On FB2.5 server "invalid transaction handle" happens on fetch.
// See also: tests/fbird_trans_012.phpt
// skip_if_fb_lt(3.0);
?>
--FILE--
<?php

require("firebird.inc");

fbird_connect($test_base);

(function() {
    var_dump($t = fbird_trans());
    var_dump(fbird_query($t, "COMMIT"));
    var_dump($t);
    var_dump(fbird_query($t, "SELECT * FROM TEST1"));
})();

?>
--EXPECTF--
object(Firebird\Transaction)#%d (0) {
}
bool(true)
object(Firebird\Transaction)#%d (0) {
}

Warning: fbird_query(): invalid transaction handle (expecting explicit transaction start)%s
bool(false)

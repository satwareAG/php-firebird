--TEST--
Issue #89: Passing result from fbird_prepare() to fbird_fetch_*() causes segfault.
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

require("firebird.inc");
fbird_connect($test_base);

(function() {
    $res = fbird_prepare('SELECT * FROM TEST1');
    var_dump(fbird_fetch_assoc($res));
})();

?>
--EXPECT--
bool(false)

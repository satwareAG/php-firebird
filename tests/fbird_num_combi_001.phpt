--TEST--
fbird_num_fields() / fbird_num_params(): combined
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

require("firebird.inc");
fbird_connect($test_base);

(function(){
    fbird_query("DELETE FROM TEST1");

    $p = fbird_prepare("INSERT INTO TEST1 (I, C) VALUES (?, ?)");
    var_dump(fbird_num_fields($p));
    var_dump(fbird_num_params($p));

    print "--------\n";

    $p = fbird_prepare("INSERT INTO TEST1 (I, C) VALUES (?, ?) RETURNING I, C");
    var_dump(fbird_num_fields($p));
    var_dump(fbird_num_params($p));
})();

?>
--EXPECT--
int(0)
int(2)
--------
int(2)
int(2)

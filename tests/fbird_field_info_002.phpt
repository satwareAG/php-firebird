--TEST--
fbird_field_info(): fields introduced in FB 3.0
--SKIPIF--
<?php
include("skipif.inc");
skip_if_fb_lt(3);
?>
--FILE--
<?php

require("firebird.inc");
fbird_connect($test_base);

(function(){
    fbird_query(file_get_contents(__DIR__."/001-FIELDS30.sql"));
    fbird_commit();

    fbird_query("INSERT INTO FIELDS30 (ID) VALUES (1)");
    $q = fbird_query("SELECT * FROM FIELDS30");
    $num_fields = fbird_num_fields($q);
    for($i = 0; $i < $num_fields; $i++){
        $info = fbird_field_info($q, $i);
        printf("%s/%s/%d\n", $info["name"], $info["type"], $info["length"]);
    }
})();

?>
--EXPECT--
ID/INTEGER/4
BOOL_FIELD/BOOLEAN/1

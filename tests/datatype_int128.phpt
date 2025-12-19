--TEST--
Check for data type INT128 (Firebird 4.0 or above)
--SKIPIF--
<?php
include("skipif.inc");
// TODO: should also check if compiled against fblient >= 4.0. Perhaps runtime
// client lib checking also needed.
skip_if_fb_lt(4.0);

if (fbird_get_client_major_version() < 4) die("skip Firebird client library < 4.0");
?>
--FILE--
<?php

    require("firebird.inc");

    $db = fbird_connect($test_base);

    fbird_query(
        "CREATE TABLE TEST_DT (
            V_INT128 INT128 NOT NULL
         )");
    fbird_commit();

    fbird_query("INSERT INTO TEST_DT (V_INT128) VALUES (1234)");
    fbird_query("INSERT INTO TEST_DT (V_INT128) VALUES (-170141183460469231731687303715884105728)");
    fbird_query("INSERT INTO TEST_DT (V_INT128) VALUES (170141183460469231731687303715884105727)");

    $sql = 'SELECT * FROM TEST_DT';
    $query = fbird_query($sql);
    while(($row = fbird_fetch_assoc($query))) {
    	var_dump($row);
    }

    fbird_free_result($query);
    fbird_close();

?>
--EXPECTF--
array(1) {
  ["V_INT128"]=>
  string(4) "1234"
}
array(1) {
  ["V_INT128"]=>
  string(40) "-170141183460469231731687303715884105728"
}
array(1) {
  ["V_INT128"]=>
  string(39) "170141183460469231731687303715884105727"
}

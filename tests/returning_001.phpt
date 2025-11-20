--TEST--
INSERT / UPDATE using RETURNING
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

require("interbase.inc");
ibase_connect($test_base);

(function(){
    ibase_query("DELETE FROM TEST1");

    $q = ibase_query("INSERT INTO TEST1 (I, C) VALUES (?, ?) RETURNING I, C", 1, "data 1");
    dump_rows($q);
    dump_table_rows("TEST1");

    print "--------\n";

    $q = ibase_query("UPDATE TEST1 SET I = I + 1, C = C || ' updated' RETURNING OLD.I, OLD.C, NEW.I, NEW.C");
    dump_rows($q);
    dump_table_rows("TEST1");
})();

?>
--EXPECT--
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(6) "data 1"
}
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(6) "data 1"
}
--------
array(4) {
  ["OLD.I"]=>
  int(1)
  ["OLD.C"]=>
  string(6) "data 1"
  ["NEW.I"]=>
  int(2)
  ["NEW.C"]=>
  string(14) "data 1 updated"
}
array(2) {
  ["I"]=>
  int(2)
  ["C"]=>
  string(14) "data 1 updated"
}

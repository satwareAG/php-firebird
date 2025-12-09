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
    var_dump(fbird_query($t, "DELETE FROM TEST1"));
    print "|---- TEST1 default transaction\n";
    dump_table_rows("TEST1");
    print "|--------\n";
    print "|---- TEST1 t1 transaction\n";
    dump_table_rows("TEST1", $t);
    print "|--------\n";
    fbird_rollback($t);
    fbird_rollback();

    print "|---- TEST1 default transaction\n";
    dump_table_rows("TEST1");
    print "|--------\n";
})();

?>
--EXPECT--
int(1)
|---- TEST1 default transaction
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(32) "test table not created with isql"
}
|--------
|---- TEST1 t1 transaction
|--------
|---- TEST1 default transaction
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(32) "test table not created with isql"
}
|--------

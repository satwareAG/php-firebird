--TEST--
fbird_trans(): transaction control with SQL
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");
fbird_connect($test_base);

(function() {
    $tr = fbird_trans();
    fbird_query($tr, "DELETE FROM TEST1");
    fbird_commit_ret($tr);
    fbird_query_bulk([
        ["INSERT INTO TEST1 (I, C) VALUES (?, ?)", [1, "test1(1)"]],
        "SAVEPOINT sp_name",
        ["INSERT INTO TEST1 (I, C) VALUES (?, ?)", [2, "test1(2)"]],
        "ROLLBACK TO SAVEPOINT sp_name",
    ], $tr);
    fbird_commit($tr);
    dump_table_rows("TEST1");
})();

?>
--EXPECT--
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(8) "test1(1)"
}

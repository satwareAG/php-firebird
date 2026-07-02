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

    $queries = [
        ["INSERT INTO TEST1 (I, C) VALUES (?, ?)", [1, "test2(1)"]],
        "SAVEPOINT sp_name",
        ["INSERT INTO TEST1 (I, C) VALUES (?, ?)", [2, "test2(2)"]],
    ];

    print "---- current status\n";
    fbird_query_bulk($queries, $tr);
    dump_table_rows("TEST1", $tr);

    print "---- now rollback\n";
    fbird_query_bulk([
        "ROLLBACK TO SAVEPOINT sp_name",
    ], $tr);
    fbird_commit($tr);
    dump_table_rows("TEST1");
})();

?>
--EXPECT--
---- current status
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(8) "test2(1)"
}
array(2) {
  ["I"]=>
  int(2)
  ["C"]=>
  string(8) "test2(2)"
}
---- now rollback
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(8) "test2(1)"
}

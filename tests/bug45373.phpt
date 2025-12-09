--TEST--
Bug #45373 (php crash on query with errors in params)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = ibase_connect($test_base);

$q = ibase_prepare($db, "SELECT * FROM TEST1 WHERE I = ? AND C = ?");
$r = ibase_execute($q, 1, 'test table not created with isql');
var_dump(ibase_fetch_assoc($r));
ibase_free_result($r);

// MUST run with error_reporting & E_NOTICE to generate Notice:
$r = ibase_execute($q, 1, 'test table not created with isql', 1);
var_dump(ibase_fetch_assoc($r));
ibase_free_result($r);

// Enforcing function parameters became more stricter in latest versions of PHP
if($r = ibase_execute($q, 1)) {
  var_dump(ibase_fetch_assoc($r));
}

echo "Done executing\n";
ibase_free_query($q);
ibase_close($db);
echo "Closed resources\n";

?>
--EXPECTF--
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(32) "test table not created with isql"
}

Notice: ibase_execute(): Statement expects 2 arguments, 3 given in %s on line %d
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(32) "test table not created with isql"
}

Warning: ibase_execute(): Statement expects 2 arguments, 1 given in %s on line %d
Done executing
Closed resources

--TEST--
Bug #45373 (php crash on query with errors in params)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

$q = fbird_prepare($db, "SELECT * FROM TEST1 WHERE I = ? AND C = ?");
$r = fbird_execute($q, 1, 'test table not created with isql');
var_dump(fbird_fetch_assoc($r));
fbird_free_result($r);

// MUST run with error_reporting & E_NOTICE to generate Notice:
$r = fbird_execute($q, 1, 'test table not created with isql', 1);
var_dump(fbird_fetch_assoc($r));
fbird_free_result($r);

// Enforcing function parameters became more stricter in latest versions of PHP
if($r = fbird_execute($q, 1)) {
  var_dump(fbird_fetch_assoc($r));
}

echo "Done executing\n";
fbird_free_query($q);
fbird_close($db);
echo "Closed resources\n";

?>
--EXPECTF--
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(32) "test table not created with isql"
}

Notice: fbird_execute(): Statement expects 2 arguments, 3 given in %s on line %d
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(32) "test table not created with isql"
}

Warning: fbird_execute(): Statement expects 2 arguments, 1 given in %s on line %d
Done executing
Closed resources

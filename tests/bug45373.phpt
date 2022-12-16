--TEST--
Bug #45373 (php crash on query with errors in params)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

	require("interbase.inc");

	$db = ibase_connect($test_base);


	$sql = "select * from test1 where i = ? and c = ?";

	$q = ibase_prepare($db, $sql);
	$r = ibase_execute($q, 1, 'test table not created with isql');
	var_dump(ibase_fetch_assoc($r));
	ibase_free_result($r);

	$r = ibase_execute($q, 1, 'test table not created with isql', 1);
	var_dump(ibase_fetch_assoc($r));
	ibase_free_result($r);

	$r = ibase_execute($q, 1);

	try {
        var_dump(ibase_fetch_assoc($r));
    } catch(TypeError $e) {
        echo $e->getMessage() . "\n";
    }
?>
--EXPECTF--
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(32) "test table not created with isql"
}

Notice: ibase_execute(): Statement expects 2 arguments, 3 given in /usr/src/php-firebird/tests/bug45373.php on line 15
array(2) {
  ["I"]=>
  int(1)
  ["C"]=>
  string(32) "test table not created with isql"
}

Warning: ibase_execute(): Statement expects 2 arguments, 1 given in %s on line %d
ibase_fetch_assoc(): Argument #1 ($result) must be of type resource, bool given

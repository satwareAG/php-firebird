--TEST--
Issue #35: fbird_prepare() fails to find table with SQL that has double quotes on table identifiers
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

function test35() {
	fbird_query('CREATE TABLE "test" (ID INTEGER, CLIENT_NAME VARCHAR(10))');
	@fbird_commit(); /* Issue #294: autocommit DDL already committed */
	$p = fbird_prepare('INSERT INTO "test" (ID, CLIENT_NAME) VALUES (?, ?)');
	fbird_execute($p, 1, "Some name");
	$q = fbird_query('SELECT * FROM "test"');
	while($r = fbird_fetch_object($q)){
		var_dump($r);
	}
}

test35();

?>
--EXPECTF--
object(stdClass)#%d (2) {
  ["ID"]=>
  int(1)
  ["CLIENT_NAME"]=>
  string(9) "Some name"
}

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE test");
    @fbird_close($db);
}
?>

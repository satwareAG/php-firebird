--TEST--
fbird_trans(): Check order of link identifier and trans args
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);
$tr = fbird_trans(FBIRD_READ, $db) or die("Could not create transaction");
@fbird_query($tr, "INSERT INTO test1 VALUES(1, 2)") or die("Could not insert");
fbird_commit($tr) or die("Could not commit transaction");
print "Finished OK\n";

unset($db);

?>
--EXPECT--
Could not insert

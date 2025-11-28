--TEST--
Reproduce Statement Reuse: Verify ibase_execute reuses prepared handle
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("interbase.inc");
$db = ibase_connect($test_base);

$sql = "SELECT 1 FROM RDB\$DATABASE";
$stmt = ibase_prepare($db, $sql);

echo "First execution:\n";
$res1 = ibase_execute($stmt);
$row1 = ibase_fetch_row($res1);
var_dump($row1[0]);
ibase_free_result($res1);

echo "Second execution (reuse):\n";
$res2 = ibase_execute($stmt);
$row2 = ibase_fetch_row($res2);
var_dump($row2[0]);
ibase_free_result($res2);

echo "Third execution (reuse):\n";
$res3 = ibase_execute($stmt);
$row3 = ibase_fetch_row($res3);
var_dump($row3[0]);
ibase_free_result($res3);

ibase_free_query($stmt);
ibase_close($db);
?>
--EXPECT--
First execution:
int(1)
Second execution (reuse):
int(1)
Third execution (reuse):
int(1)

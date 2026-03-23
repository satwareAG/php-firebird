--TEST--
fbird_fetch_* returns false without warning on EOF and closed cursor (#127)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$db = fbird_connect($test_base);

/* Test 1: fetch past end of result set — should return false, no warning */
$rs = fbird_query($db, "SELECT 1 AS id FROM RDB\$DATABASE");
$row1 = fbird_fetch_assoc($rs);
echo "row1: " . $row1['ID'] . "\n";
$row2 = fbird_fetch_assoc($rs);
var_dump($row2); /* should be false — EOF */
$row3 = fbird_fetch_assoc($rs);
var_dump($row3); /* should be false again, no warning */
fbird_free_result($rs);

/* Test 2: fetch after commit closes cursor — should return false, no warning */
$tr = fbird_trans($db);
$rs2 = fbird_query($tr, "SELECT 1 AS id FROM RDB\$DATABASE");
$r = fbird_fetch_assoc($rs2);
echo "before commit: " . $r['ID'] . "\n";
fbird_commit($tr);
$r2 = fbird_fetch_assoc($rs2);
var_dump($r2); /* should be false, no warning */

fbird_close($db);
echo "Done\n";
?>
--EXPECT--
row1: 1
bool(false)
bool(false)
before commit: 1
bool(false)
Done

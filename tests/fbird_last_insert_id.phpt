--TEST--
fbird_last_insert_id() returns current generator value (#130)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$db = fbird_connect($test_base);

$tr = fbird_trans($db);
fbird_query($tr, "RECREATE SEQUENCE test_seq_lid");
fbird_commit($tr);

/* Initial value should be 0 */
$val = fbird_last_insert_id($db, "test_seq_lid");
echo "initial: $val\n";

/* Increment via gen_id */
$new = fbird_gen_id("test_seq_lid", 1, $db);
echo "after gen_id(1): $new\n";

/* last_insert_id should now return 1 */
$val2 = fbird_last_insert_id($db, "test_seq_lid");
echo "last_insert_id: $val2\n";

/* Increment more */
fbird_gen_id("test_seq_lid", 10, $db);

$val3 = fbird_last_insert_id($db, "test_seq_lid");
echo "after gen_id(10): $val3\n";

/* Cleanup */
$tr = fbird_trans($db);
fbird_query($tr, "DROP SEQUENCE test_seq_lid");
@fbird_commit($tr);
fbird_close($db);
echo "Done\n";
?>
--EXPECT--
initial: 0
after gen_id(1): 1
last_insert_id: 1
after gen_id(10): 11
Done

--TEST--
Reproduce Commit Retaining: Verify transaction handle remains valid
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("interbase.inc");
$db = ibase_connect($test_base);

// Create a table for testing
ibase_query($db, "RECREATE TABLE test_commit_ret (id INT)");
ibase_commit($db);

// Start explicit transaction
$trans = ibase_trans($db);

echo "Insert 1:\n";
ibase_query($trans, "INSERT INTO test_commit_ret (id) VALUES (1)");

echo "Commit Retaining:\n";
// This should commit but keep $trans valid
ibase_commit_ret($trans);

echo "Insert 2 (reuse transaction handle):\n";
// If $trans was closed, this should fail
ibase_query($trans, "INSERT INTO test_commit_ret (id) VALUES (2)");

echo "Final Commit:\n";
ibase_commit($trans);

// Verify data
$check = ibase_query($db, "SELECT id FROM test_commit_ret ORDER BY id");
while ($row = ibase_fetch_object($check)) {
    echo "ID: " . $row->ID . "\n";
}
ibase_free_result($check);
ibase_commit($db); // Commit the default transaction used for reading

// Cleanup (handled by shutdown function if needed, or implicit close)
ibase_close($db);
?>
--EXPECT--
Insert 1:
Commit Retaining:
Insert 2 (reuse transaction handle):
Final Commit:
ID: 1
ID: 2

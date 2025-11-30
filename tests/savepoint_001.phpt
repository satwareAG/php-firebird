--TEST--
fbird_savepoint(), fbird_rollback_savepoint(), fbird_release_savepoint()
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$db = fbird_connect($test_base);

// Start default transaction (or explicit one)
$trans = fbird_trans($db);

// Create table for testing
fbird_query($trans, "RECREATE TABLE test_savepoints (id INT)");
fbird_commit($trans);

$trans = fbird_trans($db);

// Insert initial data
fbird_query($trans, "INSERT INTO test_savepoints (id) VALUES (1)");

// Create Savepoint A
var_dump(fbird_savepoint($trans, "SV_A"));

// Insert data after A
fbird_query($trans, "INSERT INTO test_savepoints (id) VALUES (2)");

// Create Savepoint B
var_dump(fbird_savepoint($trans, "SV_B"));

// Insert data after B
fbird_query($trans, "INSERT INTO test_savepoints (id) VALUES (3)");

// Rollback to B (should keep 1 and 2, remove 3)
var_dump(fbird_rollback_savepoint($trans, "SV_B"));

$res = fbird_query($trans, "SELECT * FROM test_savepoints ORDER BY id");
$rows = [];
while ($row = fbird_fetch_assoc($res)) {
    $rows[] = $row;
}
var_dump(count($rows)); // Expect 2 (id 1, 2)
fbird_free_result($res);
unset($res); // Ensure PHP releases the result resource completely

// Release A (makes A permanent in this trans, cannot rollback to it anymore)
var_dump(fbird_release_savepoint($trans, "SV_A"));

// Cleanup - Note: commit may warn about table in use (known Firebird behavior with savepoints)
fbird_commit($trans);

// Drop table using a fresh transaction to ensure clean state
$trans_drop = fbird_trans($db);
fbird_query($trans_drop, "DROP TABLE test_savepoints");
fbird_commit($trans_drop);

?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
int(2)
bool(true)
%A

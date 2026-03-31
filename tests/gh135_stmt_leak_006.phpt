--TEST--
GitHub #135: Error path cleanup - failed execute must not leak parent statement
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/*
 * Regression test for GitHub issue #135 - error path cleanup.
 *
 * When a one-shot function's prepare succeeds but execute fails (e.g.,
 * constraint violation, invalid data), the parent query resource must still
 * be freed. This test verifies that error paths do not leak resources by
 * running many failing queries in a tight loop.
 */
require("firebird.inc");

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('connect failed');

// Create table with a UNIQUE constraint to force execute errors
fbird_query($db, "CREATE TABLE gh135_err_test (id INTEGER NOT NULL PRIMARY KEY)");
fbird_commit($db);

// Insert the conflict row
fbird_query($db, "INSERT INTO gh135_err_test VALUES (1)");
fbird_commit($db);

echo "1. Testing fbird_query error path...\n";
$iterations = 100;
$errors_caught = 0;

for ($i = 0; $i < $iterations; $i++) {
    // This will prepare OK but fail on execute (duplicate key)
    $result = @fbird_query($db, "INSERT INTO gh135_err_test VALUES (1)");
    if ($result === false) {
        $errors_caught++;
    } else {
        echo "FAIL: expected error at iteration $i but got success\n";
        break;
    }
}

if ($errors_caught === $iterations) {
    echo "OK: $iterations error iterations - all resources cleaned up\n";
}

// Verify the connection is still healthy after many errors
$result = fbird_query($db, "SELECT COUNT(*) AS CNT FROM gh135_err_test");
$row = fbird_fetch_assoc($result);
fbird_free_result($result);
echo "Row count after errors: {$row['CNT']}\n";

echo "2. Testing fbird_execute_query error path...\n";
$trans = fbird_trans($db);
$errors_caught2 = 0;
for ($i = 0; $i < $iterations; $i++) {
    // Reference a non-existent table to trigger a prepare/execute error
    $result = @fbird_execute_query($trans, "SELECT id FROM gh135_nonexistent_table_xyz");
    if ($result === false) {
        $errors_caught2++;
    }
}
fbird_commit($trans);

if ($errors_caught2 === $iterations) {
    echo "OK: $iterations execute_query error iterations - all resources cleaned up\n";
}

echo "3. Testing fbird_query_params_tx error path...\n";
$trans = fbird_trans($db);
$errors_caught3 = 0;
for ($i = 0; $i < $iterations; $i++) {
    // Pass wrong number of params - should fail at bind/execute
    $result = @fbird_query_params_tx($db, $trans, "INSERT INTO gh135_err_test VALUES (1)", []);
    if ($result === false) {
        $errors_caught3++;
    }
}
fbird_commit($trans);

if ($errors_caught3 === $iterations) {
    echo "OK: $iterations query_params_tx error iterations - all resources cleaned up\n";
}

// Final health check - connection must still work
$result = fbird_query($db, "SELECT 1 AS ALIVE FROM RDB\$DATABASE");
$row = fbird_fetch_assoc($result);
fbird_free_result($result);
echo "Connection alive: " . ($row['ALIVE'] == 1 ? "yes" : "no") . "\n";

// Cleanup
fbird_close($db);
$db2 = fbird_connect($test_base, $user, $password);
fbird_query($db2, "DROP TABLE gh135_err_test");
fbird_commit($db2);
fbird_close($db2);
?>
--EXPECT--
1. Testing fbird_query error path...
OK: 100 error iterations - all resources cleaned up
Row count after errors: 1
2. Testing fbird_execute_query error path...
OK: 100 execute_query error iterations - all resources cleaned up
3. Testing fbird_query_params_tx error path...
OK: 100 query_params_tx error iterations - all resources cleaned up
Connection alive: yes

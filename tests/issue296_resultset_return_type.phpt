--TEST--
fbird_query() returns Firebird\ResultSet object, not raw resource (Issue #296)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #296:
 *   fbird_query() stub declares Firebird\ResultSet return type but C code
 *   returns raw resource.
 *
 * The stub in stubs/firebird-stubs.php declares:
 *   function fbird_query(mixed $link_or_query, mixed ...$args): \Firebird\ResultSet|int|bool {}
 *
 * But the C implementation returned a raw resource for SELECT queries.
 * fbird_execute() already wrapped results in Firebird\ResultSet objects
 * via fbird_setup_resultset_object(), but fbird_query() did not.
 *
 * Fix: added the same fbird_setup_resultset_object() wrapping block to
 * fbird_query(), fbird_execute_query(), and fbird_query_params_tx().
 */

$conn = fbird_connect($test_base);
if (!$conn) {
    die("FAIL: connect failed: " . fbird_errmsg() . "\n");
}

// Test 1: fbird_query() SELECT returns Firebird\ResultSet
$r1 = fbird_query($conn, 'SELECT 1 FROM RDB$DATABASE');
if (!$r1) {
    die("FAIL: fbird_query failed: " . fbird_errmsg() . "\n");
}
echo "fbird_query SELECT type: " . gettype($r1) . "\n";
echo "fbird_query SELECT instanceof ResultSet: " . ($r1 instanceof \Firebird\ResultSet ? "yes" : "no") . "\n";
fbird_free_result($r1);

// Test 2: fbird_query() DML returns int (affected rows), not object
$tx = fbird_trans($conn);
fbird_query($tx, "INSERT INTO test1 VALUES (999, 'test296')");
fbird_commit($tx);

$r2 = fbird_query($conn, "UPDATE test1 SET c = 'updated296' WHERE i = 999");
echo "fbird_query DML type: " . gettype($r2) . "\n";

// Test 3: fbird_execute_query() returns Firebird\ResultSet
$tx2 = fbird_trans($conn);
$r3 = fbird_execute_query($tx2, 'SELECT 1 FROM RDB$DATABASE');
if ($r3) {
    echo "fbird_execute_query SELECT type: " . gettype($r3) . "\n";
    echo "fbird_execute_query instanceof ResultSet: " . ($r3 instanceof \Firebird\ResultSet ? "yes" : "no") . "\n";
    fbird_free_result($r3);
}
fbird_commit($tx2);

// Test 4: fbird_query_params_tx() returns Firebird\ResultSet for SELECT
$tx3 = fbird_trans($conn);
$r4 = fbird_query_params_tx($conn, $tx3, 'SELECT 1 FROM RDB$DATABASE WHERE 1 = ?', [1]);
if ($r4) {
    echo "fbird_query_params_tx SELECT type: " . gettype($r4) . "\n";
    echo "fbird_query_params_tx instanceof ResultSet: " . ($r4 instanceof \Firebird\ResultSet ? "yes" : "no") . "\n";
    fbird_free_result($r4);
}
fbird_commit($tx3);

// Test 5: fbird_execute() still returns Firebird\ResultSet (regression check)
$stmt = fbird_prepare($conn, 'SELECT 1 FROM RDB$DATABASE');
$r5 = fbird_execute($stmt);
echo "fbird_execute SELECT type: " . gettype($r5) . "\n";
echo "fbird_execute instanceof ResultSet: " . ($r5 instanceof \Firebird\ResultSet ? "yes" : "no") . "\n";
fbird_free_result($r5);

// Cleanup
$tx4 = fbird_trans($conn);
fbird_query($tx4, 'DELETE FROM test1 WHERE i = 999');
fbird_commit($tx4);
fbird_close($conn);

echo "Done\n";
?>
--EXPECT--
fbird_query SELECT type: object
fbird_query SELECT instanceof ResultSet: yes
fbird_query DML type: integer
fbird_execute_query SELECT type: object
fbird_execute_query instanceof ResultSet: yes
fbird_query_params_tx SELECT type: object
fbird_query_params_tx instanceof ResultSet: yes
fbird_execute SELECT type: object
fbird_execute instanceof ResultSet: yes
Done

--TEST--
Nullable array params don't segfault when null is passed (Issue #307)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #307:
 *   4 functions have nullable array mismatch: arginfo says non-nullable
 *   IS_ARRAY, stubs say ?array, C parse accepts null via "a!".
 *   If null is passed, Z_ARRVAL_P may dereference NULL and segfault.
 *
 * Affected: fbird_execute_statement, fbird_execute_query,
 *           fbird_execute_auto, fbird_query_params_tx
 *
 * Fix: arginfo -> IS_ARRAY,1 (nullable) + Z_TYPE_P guard before Z_ARRVAL_P
 */

$conn = fbird_connect($test_base);
if (!$conn) die("connect failed\n");

$tx = fbird_trans($conn);

// Test 1: fbird_execute_query with null params (no segfault)
$r = fbird_execute_query($tx, 'SELECT 1 FROM RDB$DATABASE', null);
echo "execute_query null params: " . ($r !== false ? "ok" : "failed") . "\n";
if ($r) {
    $row = fbird_fetch_row($r);
    echo "result: " . $row[0] . "\n";
    fbird_free_result($r);
}
fbird_commit($tx);

// Test 2: fbird_execute_query with null params (second call for completeness)
$tx = fbird_trans($conn);
$r = fbird_execute_query($tx, 'SELECT 1 FROM RDB$DATABASE', null);
echo "execute_query null params: " . ($r !== false ? "ok" : "failed") . "\n";
if ($r) {
    $row = fbird_fetch_row($r);
    echo "result: " . $row[0] . "\n";
    fbird_free_result($r);
}
fbird_commit($tx);

// Test 3: fbird_execute_query with null params via transaction (no segfault)
$tx = fbird_trans($conn);
$r = fbird_execute_query($tx, 'SELECT 1 FROM RDB$DATABASE', null);
echo "execute_query(tx) null params: " . ($r !== false ? "ok" : "failed") . "\n";
if ($r) {
    $row = fbird_fetch_row($r);
    echo "result: " . $row[0] . "\n";
    fbird_free_result($r);
}
fbird_commit($tx);

// Test 4: fbird_query_params_tx with null params (no segfault - null = no params)
$tx = fbird_trans($conn);
$r = fbird_query_params_tx($conn, $tx, 'SELECT 1 FROM RDB$DATABASE', null);
echo "query_params_tx null params: " . ($r !== false ? "ok" : "failed") . "\n";
if ($r) {
    $row = fbird_fetch_row($r);
    echo "result: " . $row[0] . "\n";
    fbird_free_result($r);
}
fbird_commit($tx);

// Test 5: Omit params entirely (should also work)
$tx = fbird_trans($conn);
$r = fbird_execute_query($tx, 'SELECT 1 FROM RDB$DATABASE');
echo "execute_query no params: " . ($r !== false ? "ok" : "failed") . "\n";
if ($r) fbird_free_result($r);
fbird_commit($tx);

// Test 6: Pass actual array (regression check)
$tx = fbird_trans($conn);
$r = fbird_execute_query($tx, 'SELECT 1 FROM RDB$DATABASE WHERE 1 = ?', [1]);
echo "execute_query array params: " . ($r !== false ? "ok" : "failed") . "\n";
if ($r) {
    $row = fbird_fetch_row($r);
    echo "result: " . $row[0] . "\n";
    fbird_free_result($r);
}
fbird_commit($tx);

fbird_close($conn);
echo "Done\n";
?>
--EXPECTF--
execute_query null params: ok
result: 1
execute_query null params: ok
result: 1
execute_query(tx) null params: ok
result: 1
query_params_tx null params: ok
result: 1
execute_query no params: ok
execute_query array params: ok
result: 1
Done

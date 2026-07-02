--TEST--
fbird_prepare()/fbird_prepare_ex() return Firebird\Statement object (Issue #297)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #297:
 *   Complete M3 migration — return objects from fbird_prepare() and
 *   fbird_prepare_ex().
 *
 * Previously, fbird_prepare() and fbird_prepare_ex() returned raw
 * resource handles. The Firebird\Statement class existed as a skeleton
 * since v8.0.0 but was never returned by these functions.
 *
 * Fix: added fbird_setup_statement_object() helper (same pattern as
 * fbird_setup_resultset_object()) and call it at the end of both
 * functions to wrap the le_query resource in a Firebird\Statement object.
 *
 * The dual-accept bridge (M3 Phase E/F) already accepts Firebird\Statement
 * objects in all consuming functions (fbird_execute, fbird_free_query, etc.),
 * so no consuming-side changes are needed.
 */

$conn = fbird_connect($test_base);
if (!$conn) {
    die("FAIL: connect failed: " . fbird_errmsg() . "\n");
}

// Test 1: fbird_prepare() returns Firebird\Statement
$stmt1 = fbird_prepare($conn, 'SELECT 1 FROM RDB$DATABASE');
if (!$stmt1) {
    die("FAIL: fbird_prepare failed: " . fbird_errmsg() . "\n");
}
echo "fbird_prepare type: " . gettype($stmt1) . "\n";
echo "fbird_prepare instanceof Statement: " . ($stmt1 instanceof \Firebird\Statement ? "yes" : "no") . "\n";

// Test 2: fbird_prepare() result works with fbird_execute() (dual-accept bridge)
$r1 = fbird_execute($stmt1);
if (!$r1) {
    die("FAIL: fbird_execute on Statement object failed: " . fbird_errmsg() . "\n");
}
echo "fbird_execute on Statement type: " . gettype($r1) . "\n";
echo "fbird_execute instanceof ResultSet: " . ($r1 instanceof \Firebird\ResultSet ? "yes" : "no") . "\n";
fbird_free_result($r1);

// Test 3: fbird_prepare_ex() returns Firebird\Statement
$stmt2 = fbird_prepare_ex($conn, 'SELECT 2 FROM RDB$DATABASE');
if (!$stmt2) {
    die("FAIL: fbird_prepare_ex failed: " . fbird_errmsg() . "\n");
}
echo "fbird_prepare_ex type: " . gettype($stmt2) . "\n";
echo "fbird_prepare_ex instanceof Statement: " . ($stmt2 instanceof \Firebird\Statement ? "yes" : "no") . "\n";

// Test 4: fbird_prepare_ex() result works with fbird_execute()
$r2 = fbird_execute($stmt2);
if (!$r2) {
    die("FAIL: fbird_execute on Statement (ex) failed: " . fbird_errmsg() . "\n");
}
$row = fbird_fetch_row($r2);
echo "fbird_prepare_ex result value: " . $row[0] . "\n";
fbird_free_result($r2);

// Test 5: Re-execute the prepared statement (Statement object is reusable)
$r3 = fbird_execute($stmt1);
$row3 = fbird_fetch_row($r3);
echo "re-execute result value: " . $row3[0] . "\n";
fbird_free_result($r3);

// Test 6: fbird_prepare() with explicit transaction
$tx = fbird_trans($conn);
$stmt3 = fbird_prepare($tx, 'SELECT 3 FROM RDB$DATABASE');
echo "fbird_prepare with tx type: " . gettype($stmt3) . "\n";
echo "fbird_prepare with tx instanceof Statement: " . ($stmt3 instanceof \Firebird\Statement ? "yes" : "no") . "\n";
$r4 = fbird_execute($stmt3);
$row4 = fbird_fetch_row($r4);
echo "fbird_prepare with tx result value: " . $row4[0] . "\n";
fbird_free_result($r4);
fbird_commit($tx);

fbird_close($conn);
echo "Done\n";
?>
--EXPECT--
fbird_prepare type: object
fbird_prepare instanceof Statement: yes
fbird_execute on Statement type: object
fbird_execute instanceof ResultSet: yes
fbird_prepare_ex type: object
fbird_prepare_ex instanceof Statement: yes
fbird_prepare_ex result value: 2
re-execute result value: 1
fbird_prepare with tx type: object
fbird_prepare with tx instanceof Statement: yes
fbird_prepare with tx result value: 3
Done

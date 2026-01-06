--TEST--
Shutdown persistent connection with nested resources - safety test
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for shutdown segfault prevention with persistent connections:
 * Tests that pconnect resources are safely cleaned up during MSHUTDOWN.
 *
 * Persistent connections have a different cleanup path than regular connections.
 * This test creates multiple resources on a persistent connection and verifies
 * that the MSHUTDOWN guards properly prevent SIGSEGV.
 *
 * Expected behavior: Clean exit (exit code 0) with "Test completed" output.
 * Failure mode: SIGSEGV crash (exit code 139) during MSHUTDOWN.
 *
 * Related: Issue #50/#51 (persitent link MSHUTDOWN guards)
 */

// Connect using persistent connection
$db = fbird_pconnect($test_base);
if (!$db) {
    die("Connection failed: " . fbird_errmsg());
}

// Create a transaction on the persistent connection
$trans = fbird_trans($db);
if (!$trans) {
    die("Transaction failed: " . fbird_errmsg());
}

// Prepare multiple queries (not freed)
$q1 = fbird_prepare($db, "SELECT 1 FROM RDB\$DATABASE");
$q2 = fbird_prepare($db, "SELECT 2 FROM RDB\$DATABASE");

if (!$q1 || !$q2) {
    die("Prepare failed: " . fbird_errmsg());
}

// Execute first query
$r1 = fbird_execute($q1);
if (!$r1) {
    die("Execute q1 failed: " . fbird_errmsg());
}

// Execute second query
$r2 = fbird_execute($q2);
if (!$r2) {
    die("Execute q2 failed: " . fbird_errmsg());
}

// Fetch from both to ensure they're active
$row1 = fbird_fetch_row($r1);
$row2 = fbird_fetch_row($r2);

if ($row1 === false || $row2 === false) {
    die("Fetch failed: " . fbird_errmsg());
}

// Verify results
if ($row1[0] != 1 || $row2[0] != 2) {
    die("Unexpected results");
}

// IMPORTANT: Do NOT call any cleanup functions here!
// The purpose is to verify that pconnect cleanup during MSHUTDOWN
// handles nested resources (transactions, queries, results) safely.

echo "Test completed\n";
?>
--EXPECT--
Test completed

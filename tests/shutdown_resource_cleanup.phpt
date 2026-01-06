--TEST--
Shutdown resource cleanup - basic safety (no SIGSEGV on unfreed resources)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for shutdown segfault prevention:
 * Tests that PHP gracefully handles unfree'd resources at shutdown.
 *
 * This test creates various resources (connection, transaction, query)
 * and intentionally does NOT free them. The resource destructors must
 * handle this safely during PHP shutdown.
 *
 * Expected behavior: Clean exit (exit code 0) with "Test completed" output.
 * Failure mode: SIGSEGV crash (exit code 139) during shutdown.
 *
 * Related: Issue #56 (fork safety), Issue #50/#51 (MSHUTDOWN guards)
 */

// Create connection
$db = fbird_connect($test_base);
if (!$db) {
    die("Connection failed: " . fbird_errmsg());
}

// Create transaction (not committed/rolled back explicitly)
$trans = fbird_trans($db);
if (!$trans) {
    die("Transaction failed: " . fbird_errmsg());
}

// Prepare a query (not freed)
$query = fbird_prepare($db, "SELECT 1 FROM RDB\$DATABASE");
if (!$query) {
    die("Prepare failed: " . fbird_errmsg());
}

// Execute query (result not freed)
$result = fbird_execute($query);
if (!$result) {
    die("Execute failed: " . fbird_errmsg());
}

// Fetch to ensure result is in use
$row = fbird_fetch_row($result);
if ($row === false) {
    die("Fetch failed: " . fbird_errmsg());
}

// Verify data
if ($row[0] != 1) {
    die("Unexpected result: " . var_export($row, true));
}

// IMPORTANT: Do NOT call fbird_free_result(), fbird_free_query(),
// fbird_rollback(), or fbird_close() here!
// The purpose of this test is to verify that all resource destructors
// handle shutdown correctly when resources are not explicitly freed.

echo "Test completed\n";
?>
--EXPECT--
Test completed

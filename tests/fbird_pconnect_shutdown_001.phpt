--TEST--
fbird_pconnect() - clean shutdown without SIGSEGV (Issue #50, #51)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issues #50 and #51:
 * - SIGSEGV (exit code 139) during PHP shutdown
 * - EG() access during MSHUTDOWN causing crash
 *
 * Root cause: _php_fbird_close_plink() attempted to access EG(regular_list)
 * and EG(persistent_list) during MSHUTDOWN when these may already be destroyed.
 *
 * Fix: Added in_mshutdown flag to module globals that guards EG() access
 * in the persistent link destructor.
 *
 * Expected behavior: Clean exit (exit code 0) with "Test completed" output.
 * Failure mode: SIGSEGV crash (exit code 139) before reaching "Test completed".
 */

// Connect using persistent connection
$db = fbird_pconnect($test_base);

if (!$db) {
    die("Connection failed: " . fbird_errmsg());
}

// Perform a simple query to ensure connection is active
$result = fbird_query($db, 'SELECT 1 FROM RDB$DATABASE');
if (!$result) {
    die("Query failed: " . fbird_errmsg());
}

$row = fbird_fetch_row($result);
if ($row === false) {
    die("Fetch failed: " . fbird_errmsg());
}

fbird_free_result($result);

// IMPORTANT: Do NOT call fbird_close() here!
// The purpose of this test is to verify that the persistent connection
// destructor (_php_fbird_close_plink) handles shutdown correctly when
// called during PHP's module shutdown phase.

echo "Test completed\n";
?>
--EXPECT--
Test completed
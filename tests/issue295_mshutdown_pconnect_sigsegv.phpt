--TEST--
fbird_pconnect: MSHUTDOWN SIGSEGV with active default transaction (Issue #295)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #295:
 *   SIGSEGV during PHP shutdown after persistent connection cleanup (v11.0.1)
 *
 * Root cause:
 *   Transaction::commit() / rollback() / rollbackNoThrow() in src/cpp/fb_transaction.hpp
 *   guard on the cached master_ member instead of calling getMaster(). During MSHUTDOWN,
 *   getMaster() returns nullptr (IBG(in_mshutdown) is set), but master_ is never nulled,
 *   so the guard is bypassed and a server-side ITransaction::commit() call is made on a
 *   dead attachment -> SIGSEGV (exit code 139).
 *
 *   This is the same class of bug that commit 881d375 fixed for Connection::detachNoThrow(),
 *   but the fix was never applied to the Transaction class.
 *
 * Crash chain:
 *   _php_fbird_close_plink()                         [fbird_connection.c:264]
 *     -> _php_fbird_commit_link()                    [fbird_connection.c:78]
 *       -> fbt_commit(default_tx)                    [fbird_connection.c:92]
 *         -> Transaction::commit()                   [firebird_utils.cpp:817]
 *           -> transaction_->commit()                [fb_transaction.hpp:277]  <- SIGSEGV
 *
 * This test exercises the crash path:
 *   1. Opens a persistent connection (le_plink)
 *   2. Starts the DEFAULT transaction via fbird_query($conn, ...) autocommit
 *   3. Does NOT free the result (keeps the default transaction's fbt_transaction non-null)
 *   4. Does NOT close the persistent connection
 *   5. cleanup_db() (registered by firebird.inc) attempts to drop the DB from a separate
 *      connection before MSHUTDOWN - if the drop succeeds, the pconnect's server-side
 *      attachment is dead, and MSHUTDOWN's fbt_commit() on the dead attachment crashes.
 *
 * Expected: clean exit (exit code 0) with "ok" output.
 * Failure: SIGSEGV (exit code 139) during MSHUTDOWN.
 */

$conn = fbird_pconnect($test_base);
if (!$conn) {
    die("FAIL: pconnect failed: " . fbird_errmsg() . "\n");
}

// Start the DEFAULT transaction (first tr_list node) via autocommit query.
// This is the crash path - _php_fbird_commit_link commits this transaction during MSHUTDOWN.
$result = fbird_query($conn, 'SELECT 1 FROM RDB$DATABASE');
if (!$result) {
    die("FAIL: query failed: " . fbird_errmsg() . "\n");
}

$row = fbird_fetch_row($result);
if (!$row || $row[0] != 1) {
    die("FAIL: unexpected query result\n");
}

// Intentionally do NOT call fbird_free_result($result).
// The default transaction's fbt_transaction stays non-null.
// Intentionally do NOT call fbird_close($conn).
// The persistent connection survives until MSHUTDOWN cleanup.

echo "ok\n";
// Script ends -> cleanup_db() runs -> MSHUTDOWN destroys persistent connection
?>
--EXPECT--
ok

--TEST--
fbird_transaction: _php_fbird_free_trans() must not crash during MSHUTDOWN (issues #78, #79)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
$conn = @fbird_connect(
    getenv('FBIRD_TEST_DB') ?: '127.0.0.1/3050:/var/lib/firebird/data/test.fdb',
    getenv('FBIRD_TEST_USER') ?: 'SYSDBA',
    getenv('FBIRD_TEST_PASS') ?: 'masterkey'
);
if (!$conn) die('skip cannot connect to Firebird');
fbird_close($conn);
?>
--FILE--
<?php
/**
 * Regression test for issues #78 and #79:
 *
 * #78: Missing in_mshutdown guard in _php_fbird_free_trans() causes
 *      SIGABRT when _php_fbird_error() accesses EG() during MSHUTDOWN.
 *
 * #79: Use-After-Free in _php_fbird_free_trans() — db_link[] traversal
 *      during MSHUTDOWN accesses freed connection data (dangling pointer).
 *
 * The test verifies that a script which:
 *   1. Opens a persistent connection (le_plink)
 *   2. Starts an explicit transaction linked to that connection
 *   3. Lets both resources go out of scope without explicit commit/rollback
 *
 * ...exits cleanly (exit code 0, no SIGSEGV/SIGABRT).
 *
 * If the bug is present, PHP will crash with exit code 139 (SIGSEGV)
 * or exit code 134 (SIGABRT) during MSHUTDOWN resource cleanup.
 */

$db = getenv('FBIRD_TEST_DB') ?: '127.0.0.1/3050:/var/lib/firebird/data/test.fdb';
$user = getenv('FBIRD_TEST_USER') ?: 'SYSDBA';
$pass = getenv('FBIRD_TEST_PASS') ?: 'masterkey';

// Use a persistent connection so le_plink is registered.
// During MSHUTDOWN, le_plink destructors run before le_trans destructors
// in some orderings, creating the dangling pointer condition (#79).
$conn = fbird_pconnect($db, $user, $pass);
if (!$conn) {
    die('FAIL: could not connect');
}

// Start an explicit transaction linked to the persistent connection.
// This transaction will NOT be committed/rolled back explicitly —
// _php_fbird_free_trans() must handle cleanup safely during MSHUTDOWN.
$tx = fbird_trans(FBIRD_DEFAULT, $conn);
if (!$tx) {
    die('FAIL: could not start transaction');
}

// Verify the transaction is functional before we let it go out of scope.
$result = fbird_query($tx, 'SELECT 1 FROM RDB$DATABASE');
if (!$result) {
    die('FAIL: query failed');
}
$row = fbird_fetch_row($result);
if (!$row || $row[0] != 1) {
    die('FAIL: unexpected query result');
}
fbird_free_result($result);

// Intentionally do NOT commit or rollback $tx.
// Let PHP's MSHUTDOWN handle cleanup — this is the crash scenario.
// Both $conn (le_plink) and $tx (le_trans) will be destroyed by the
// resource destructor during shutdown.

echo "ok\n";
// Script ends here — MSHUTDOWN will run _php_fbird_free_trans()
?>
--EXPECT--
ok

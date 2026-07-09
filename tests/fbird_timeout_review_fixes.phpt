--TEST--
Firebird 4.0+ timeout review fixes: resource type validation + negative value rejection (#422 review)
--SKIPIF--
<?php
require_once 'skipif.inc';
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('STATEMENT_TIMEOUT')) die('skip STATEMENT_TIMEOUT not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

echo "=== Test 1: Transaction resource rejected (not type-confused as connection) ===\n";
$trans = fbird_trans($db);

/* Before the fix, passing a transaction resource caused type confusion:
 * fbird_transaction* was reinterpreted as fbird_db_link*, and the code
 * read an arbitrary field as fbc_connection, leading to a crash.
 * After the fix, passing a non-connection (object or resource) is rejected
 * safely without a crash. */
$result = @fbird_set_statement_timeout($trans, 5000);
echo "set_statement_timeout(trans, 5000): " . ($result ? 'true' : 'false') . "\n";
echo "fbird_errmsg: " . fbird_errmsg() . "\n";

/* Verify the connection is still usable (no crash, no corruption) */
$ok = fbird_set_statement_timeout($db, 3000);
echo "set_statement_timeout(conn, 3000) after bad call: " . ($ok ? 'true' : 'false') . "\n";

fbird_rollback($trans);

echo "\n=== Test 2: Negative statement timeout throws ValueError (procedural) ===\n";
try {
    fbird_set_statement_timeout($db, -1);
    echo "FAIL: no exception thrown\n";
} catch (\ValueError $e) {
    echo "ValueError: " . $e->getMessage() . "\n";
}

echo "\n=== Test 3: Negative idle timeout throws ValueError (procedural) ===\n";
try {
    fbird_set_idle_timeout($db, -100);
    echo "FAIL: no exception thrown\n";
} catch (\ValueError $e) {
    echo "ValueError: " . $e->getMessage() . "\n";
}

echo "\n=== Test 4: Negative statement timeout throws ValueError (OOP) ===\n";
$conn = new Firebird\Connection($test_base, $user, $password);
try {
    $conn->setStatementTimeout(-1);
    echo "FAIL: no exception thrown\n";
} catch (\ValueError $e) {
    echo "ValueError: " . $e->getMessage() . "\n";
}

echo "\n=== Test 5: Negative idle timeout throws ValueError (OOP) ===\n";
try {
    $conn->setIdleTimeout(-50);
    echo "FAIL: no exception thrown\n";
} catch (\ValueError $e) {
    echo "ValueError: " . $e->getMessage() . "\n";
}

echo "\n=== Test 6: Zero is accepted (disables timeout) ===\n";
$ok = fbird_set_statement_timeout($db, 0);
echo "set_statement_timeout(0): " . ($ok ? 'true' : 'false') . "\n";
$ok = $conn->setIdleTimeout(0);
echo "setIdleTimeout(0): " . ($ok ? 'true' : 'false') . "\n";

/* Cleanup */
fbird_close($db);
$conn->close();
unset($conn);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Transaction resource rejected (not type-confused as connection) ===
set_statement_timeout(trans, 5000): false
fbird_errmsg: No valid connection
set_statement_timeout(conn, 3000) after bad call: true

=== Test 2: Negative statement timeout throws ValueError (procedural) ===
ValueError: fbird_set_statement_timeout(): Argument #2 ($milliseconds) must be non-negative, %s given

=== Test 3: Negative idle timeout throws ValueError (procedural) ===
ValueError: fbird_set_idle_timeout(): Argument #2 ($seconds) must be non-negative, %s given

=== Test 4: Negative statement timeout throws ValueError (OOP) ===
ValueError: Firebird\Connection::setStatementTimeout(): Argument #1 ($milliseconds) must be non-negative, %s given

=== Test 5: Negative idle timeout throws ValueError (OOP) ===
ValueError: Firebird\Connection::setIdleTimeout(): Argument #1 ($seconds) must be non-negative, %s given

=== Test 6: Zero is accepted (disables timeout) ===
set_statement_timeout(0): true
setIdleTimeout(0): true

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

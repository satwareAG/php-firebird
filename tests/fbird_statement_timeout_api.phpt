--TEST--
Firebird 4.0+ statement/session timeout full API (#422 approach B)
--SKIPIF--
<?php
require_once 'skipif.inc';
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('STATEMENT_TIMEOUT')) die('skip STATEMENT_TIMEOUT not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

echo "=== Test 1: Procedural set/get statement timeout ===\n";
$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

$ok = fbird_set_statement_timeout($db, 5000);
echo "set_statement_timeout(5000): " . ($ok ? 'true' : 'false') . "\n";
$val = fbird_get_statement_timeout($db);
echo "get_statement_timeout: " . $val . "\n";

echo "\n=== Test 2: Procedural set/get idle timeout ===\n";
$ok = fbird_set_idle_timeout($db, 60);
echo "set_idle_timeout(60): " . ($ok ? 'true' : 'false') . "\n";
$val = fbird_get_idle_timeout($db);
echo "get_idle_timeout: " . $val . "\n";

fbird_close($db);

echo "\n=== Test 3: PDO set/get statement timeout ===\n";
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::FBIRD_ATTR_STATEMENT_TIMEOUT, 3000);
echo "setAttribute(STATEMENT_TIMEOUT, 3000): ok\n";
$val = $pdo->getAttribute(PDO::FBIRD_ATTR_STATEMENT_TIMEOUT);
echo "getAttribute(STATEMENT_TIMEOUT): " . $val . "\n";

echo "\n=== Test 4: PDO set/get idle timeout ===\n";
$pdo->setAttribute(PDO::FBIRD_ATTR_IDLE_TIMEOUT, 120);
echo "setAttribute(IDLE_TIMEOUT, 120): ok\n";
$val = $pdo->getAttribute(PDO::FBIRD_ATTR_IDLE_TIMEOUT);
echo "getAttribute(IDLE_TIMEOUT): " . $val . "\n";

echo "\n=== Test 5: OOP set/get statement timeout ===\n";
$conn = new Firebird\Connection(
    $test_base, $user, $password
);
$ok = $conn->setStatementTimeout(2000);
echo "setStatementTimeout(2000): " . ($ok ? 'true' : 'false') . "\n";
$val = $conn->getStatementTimeout();
echo "getStatementTimeout: " . $val . "\n";

echo "\n=== Test 6: OOP set/get idle timeout ===\n";
$ok = $conn->setIdleTimeout(30);
echo "setIdleTimeout(30): " . ($ok ? 'true' : 'false') . "\n";
$val = $conn->getIdleTimeout();
echo "getIdleTimeout: " . $val . "\n";

echo "\n=== Test 7: Statement timeout enforcement ===\n";
/* Set a very short timeout (1ms) and run a slow query */
$db2 = fbird_connect($test_base, $user, $password);
fbird_set_statement_timeout($db2, 1);
$trans = fbird_trans($db2);
$timed_out = false;
$result = @fbird_query($trans, "SELECT r1.rdb\$field_name, r2.rdb\$field_name
    FROM rdb\$fields r1 CROSS JOIN rdb\$fields r2
    CROSS JOIN rdb\$fields r3
    WHERE r1.rdb\$field_id + r2.rdb\$field_id + r3.rdb\$field_id > 0");
if (!$result) {
    $timed_out = true;
} else {
    fbird_free_result($result);
}
fbird_rollback($trans);
echo "Statement timed out: " . ($timed_out ? 'yes' : 'no') . "\n";

/* Cleanup */
fbird_close($db2);
$conn->close();
unset($pdo);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Procedural set/get statement timeout ===
set_statement_timeout(5000): true
get_statement_timeout: 5000

=== Test 2: Procedural set/get idle timeout ===
set_idle_timeout(60): true
get_idle_timeout: 60

=== Test 3: PDO set/get statement timeout ===
setAttribute(STATEMENT_TIMEOUT, 3000): ok
getAttribute(STATEMENT_TIMEOUT): 3000

=== Test 4: PDO set/get idle timeout ===
setAttribute(IDLE_TIMEOUT, 120): ok
getAttribute(IDLE_TIMEOUT): 120

=== Test 5: OOP set/get statement timeout ===
setStatementTimeout(2000): true
getStatementTimeout: 2000

=== Test 6: OOP set/get idle timeout ===
setIdleTimeout(30): true
getIdleTimeout: 30

=== Test 7: Statement timeout enforcement ===
Statement timed out: %s

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

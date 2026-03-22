--TEST--
pdo_fbird: transaction commit and rollback
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!extension_loaded('pdo_fbird')) die('skip pdo_fbird not loaded');
if (!extension_loaded('firebird')) die('skip firebird not loaded');
require_once __DIR__ . '/../../tests/firebird.inc';
if (!@fbird_connect($test_base, $user, $password)) die('skip cannot connect to Firebird');
?>
--FILE--
<?php
require_once __DIR__ . '/../../tests/firebird.inc';

if (strpos($test_base, ':') !== false) {
    [$dsn_host, $dsn_db] = explode(':', $test_base, 2);
} else { $dsn_host = 'localhost'; $dsn_db = $test_base; }

$pdo = new PDO("fbird:host={$dsn_host};dbname={$dsn_db}", $user, $password,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION, PDO::ATTR_AUTOCOMMIT => 0]);

// Create table (DDL needs explicit transaction in Firebird)
$pdo->beginTransaction();
$pdo->exec("CREATE TABLE PDO_TX_TEST (ID INTEGER)");
$pdo->commit();

// Insert and commit
$pdo->beginTransaction();
$pdo->exec("INSERT INTO PDO_TX_TEST (ID) VALUES (1)");
$pdo->commit();

// Insert and rollback
$pdo->beginTransaction();
$pdo->exec("INSERT INTO PDO_TX_TEST (ID) VALUES (2)");
$pdo->rollBack();

// Check only 1 row exists
$pdo->beginTransaction();
$stmt = $pdo->query("SELECT COUNT(*) AS CNT FROM PDO_TX_TEST");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
$stmt = null;
echo "COUNT=" . $row['CNT'] . "\n";
$pdo->commit();

// Cleanup
$pdo->beginTransaction();
$pdo->exec("DROP TABLE PDO_TX_TEST");
$pdo->commit();

$pdo = null;
echo "ok\n";
?>
--EXPECT--
COUNT=1
ok

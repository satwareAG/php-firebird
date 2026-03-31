--TEST--
pdo_fbird: transaction commit and rollback behavior
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

$pdo->exec("RECREATE TABLE pdo_txn_test (val INTEGER)");

// Commit works
$pdo->beginTransaction();
$pdo->exec("INSERT INTO pdo_txn_test (val) VALUES (1)");
$pdo->commit();

$stmt = $pdo->query("SELECT COUNT(*) AS CNT FROM pdo_txn_test");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "After commit: " . $row['CNT'] . "\n";

$pdo->exec("DROP TABLE pdo_txn_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
After commit: 1
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
@$pdo->exec("DROP TABLE pdo_txn_test");
unset($pdo);
?>

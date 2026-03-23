--TEST--
PDO_FBIRD: Transaction isolation level behavior verification
--SKIPIF--
<?php
if (!extension_loaded('PDO')) die('skip PDO not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';

$pdo = pdo_fbird_connect();

$pdo->exec("RECREATE TABLE txn_iso_beh (id INTEGER, val VARCHAR(50))");
$pdo->exec("INSERT INTO txn_iso_beh VALUES (1, 'initial')");

// Test 1: READ COMMITTED — default, data visible after own commit
$pdo->setAttribute(PDO::ATTR_AUTOCOMMIT, 0);
$pdo->beginTransaction();
$pdo->exec("UPDATE txn_iso_beh SET val = 'rc_updated' WHERE id = 1");
$stmt = $pdo->query("SELECT val FROM txn_iso_beh WHERE id = 1");
echo "in-txn read: " . $stmt->fetchColumn() . "\n";
$stmt->closeCursor();
$pdo->commit();

$pdo->beginTransaction();
$stmt = $pdo->query("SELECT val FROM txn_iso_beh WHERE id = 1");
echo "after commit: " . $stmt->fetchColumn() . "\n";
$stmt->closeCursor();
$pdo->commit();

// Test 2: Rollback actually reverts
$pdo->beginTransaction();
$pdo->exec("UPDATE txn_iso_beh SET val = 'should_revert' WHERE id = 1");
$pdo->rollBack();

$pdo->beginTransaction();
$stmt = $pdo->query("SELECT val FROM txn_iso_beh WHERE id = 1");
echo "after rollback: " . $stmt->fetchColumn() . "\n";
$stmt->closeCursor();
$pdo->commit();

// Cleanup
$pdo->setAttribute(PDO::ATTR_AUTOCOMMIT, 1);
$pdo->exec("DROP TABLE txn_iso_beh");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
in-txn read: rc_updated
after commit: rc_updated
after rollback: rc_updated
Done

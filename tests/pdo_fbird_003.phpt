--TEST--
pdo_fbird: transaction commit and rollback
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

$pdo->exec("RECREATE TABLE pdo_test3 (val INTEGER)");
$pdo->beginTransaction();
$pdo->exec("INSERT INTO pdo_test3 (val) VALUES (42)");
$pdo->commit();

$stmt = $pdo->query("SELECT COUNT(*) AS CNT FROM pdo_test3");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "COUNT=" . $row['CNT'] . "\n";

$pdo->exec("DROP TABLE pdo_test3");
$pdo = null;
echo "ok\n";
?>
--EXPECT--
COUNT=1
ok

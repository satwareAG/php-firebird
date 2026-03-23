--TEST--
pdo_fbird: basic query and fetch
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

$stmt = $pdo->query("SELECT 1 AS VAL FROM RDB\$DATABASE");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "VAL=" . $row['VAL'] . "\n";
$stmt = null;

$stmt2 = $pdo->query("SELECT 'hello' AS GREETING FROM RDB\$DATABASE");
$row2 = $stmt2->fetch(PDO::FETCH_ASSOC);
echo "GREETING=" . trim($row2['GREETING']) . "\n";
$stmt2 = null;

$pdo = null;
echo "ok\n";
?>
--EXPECT--
VAL=1
GREETING=hello
ok

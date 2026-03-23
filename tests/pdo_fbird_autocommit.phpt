--TEST--
pdo_fbird: autocommit mode — basic attribute check
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

// Default autocommit should be on
echo "Autocommit: " . ($pdo->getAttribute(PDO::ATTR_AUTOCOMMIT) ? "on" : "off") . "\n";

// Basic query works in autocommit mode
$stmt = $pdo->query("SELECT 1 AS VAL FROM RDB\$DATABASE");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Query works: " . ($row['VAL'] == 1 ? "yes" : "no") . "\n";
$stmt = null;

$pdo = null;
echo "Done\n";
?>
--EXPECT--
Autocommit: on
Query works: yes
Done

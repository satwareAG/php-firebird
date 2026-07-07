--TEST--
PDO Definition: driver registration (PDO::getAvailableDrivers)
--CREDITS--
v12.1.0 M1-22 (#349)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Driver registration ===\n";

// Static call
$drivers = PDO::getAvailableDrivers();
echo "OK static getAvailableDrivers: " . (in_array('fbird', $drivers) ? 'fbird present' : 'MISSING') . "\n";

// Instance call
$pdo = pdo_fbird_connect();
$drivers2 = $pdo->getAvailableDrivers();
echo "OK instance getAvailableDrivers: " . (in_array('fbird', $drivers2) ? 'fbird present' : 'MISSING') . "\n";

// List all drivers (varies by PHP config - don't hardcode)
echo "OK driver count: " . count($drivers) . "\n";

echo "=== DONE ===\n";
?>
--EXPECTF--
=== Driver registration ===
OK static getAvailableDrivers: fbird present
OK instance getAvailableDrivers: fbird present
OK driver count: %d
=== DONE ===

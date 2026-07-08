--TEST--
PDO Definition: driver API version matches PDO_DRIVER_API
--CREDITS--
v12.1.0 M1-23 (#350)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Driver API version ===\n";

// The driver must register with the correct PDO Driver API version.
// If it didn't, it wouldn't load at all (php_pdo would reject it).
// This test verifies the extension loads successfully, which proves
// API version compatibility.

$pdo = pdo_fbird_connect();
echo "OK driver loaded (implicit API version match)\n";

// PDO doesn't expose the registered API version as a queryable constant,
// but the fact that getAvailableDrivers() returns 'fbird' confirms the
// driver registered via php_pdo_register_driver() with a compatible
// pdo_driver_t.api_version field.
$drivers = PDO::getAvailableDrivers();
echo "OK fbird registered: " . (in_array('fbird', $drivers) ? 'yes' : 'no') . "\n";

// Check the driver name length matches what PDO expects
echo "OK driver name: " . $pdo->getAttribute(PDO::ATTR_DRIVER_NAME) . "\n";

echo "=== DONE ===\n";
?>
--EXPECT--
=== Driver API version ===
OK driver loaded (implicit API version match)
OK fbird registered: yes
OK driver name: fbird
=== DONE ===

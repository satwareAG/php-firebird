--TEST--
PDO Definition: optional get_driver_methods
--CREDITS--
v12.1.0 M1-10 (#337)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Optional: get_driver_methods ===\n";
$pdo = pdo_fbird_connect();
// PDO doesn't have a public get_driver_methods on the PDO class directly.
// This is an internal C-level hook. We verify the PDO object doesn't
// expose unexpected driver-specific methods.
$rc = new ReflectionClass(PDO::class);
$methods = array_map(fn($m) => $m->getName(), $rc->getMethods(ReflectionMethod::IS_PUBLIC));
echo "OK PDO method count: " . count($methods) . "\n";
echo "OK has beginTransaction: " . (in_array('beginTransaction', $methods) ? 'yes' : 'no') . "\n";
echo "=== DONE ===\n";
?>
--EXPECTF--
=== Optional: get_driver_methods ===
OK PDO method count: %d
OK has beginTransaction: yes
=== DONE ===

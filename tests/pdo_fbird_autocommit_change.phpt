--TEST--
pdo_fbird: switch autocommit mode mid-connection
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

echo "Initial autocommit: " . ($pdo->getAttribute(PDO::ATTR_AUTOCOMMIT) ? "on" : "off") . "\n";

// Turn off autocommit
$pdo->setAttribute(PDO::ATTR_AUTOCOMMIT, false);
echo "After disable: " . ($pdo->getAttribute(PDO::ATTR_AUTOCOMMIT) ? "on" : "off") . "\n";

// Turn autocommit back on
$pdo->setAttribute(PDO::ATTR_AUTOCOMMIT, true);
echo "After re-enable: " . ($pdo->getAttribute(PDO::ATTR_AUTOCOMMIT) ? "on" : "off") . "\n";

// Query still works after toggling
$stmt = $pdo->query("SELECT 1 AS VAL FROM RDB\$DATABASE");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Query works: " . ($row['VAL'] == 1 ? "yes" : "no") . "\n";
$stmt = null;

$pdo = null;
echo "Done\n";
?>
--EXPECT--
Initial autocommit: on
After disable: off
After re-enable: on
Query works: yes
Done

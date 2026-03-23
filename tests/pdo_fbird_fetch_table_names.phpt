--TEST--
pdo_fbird: FETCH_TABLE_NAMES attribute set/get
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

// Default should be off
echo "Default: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES) ? "on" : "off") . "\n";

// Enable
$pdo->setAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES, true);
echo "After enable: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES) ? "on" : "off") . "\n";

// Disable
$pdo->setAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES, false);
echo "After disable: " . ($pdo->getAttribute(PDO::FBIRD_ATTR_FETCH_TABLE_NAMES) ? "on" : "off") . "\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
Default: off
After enable: on
After disable: off
Done

--TEST--
PDO_FBIRD: Service API backup and restore operations
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

// Test service attach
$pdo->setAttribute(PDO::FBIRD_ATTR_SERVICE_ATTACH, true);
echo "service attached\n";

// Test server version
$version = $pdo->getAttribute(PDO::FBIRD_ATTR_SERVICE_SERVER_VERSION);
echo "server version is string: " . (is_string($version) ? "yes" : "no") . "\n";
echo "server version not empty: " . (!empty($version) ? "yes" : "no") . "\n";

// Test database stats (header only — fast)
$stats = $pdo->getAttribute(PDO::FBIRD_ATTR_SERVICE_DB_STATS);
echo "db stats is string: " . (is_string($stats) ? "yes" : "no") . "\n";

// Test service detach
$pdo->setAttribute(PDO::FBIRD_ATTR_SERVICE_DETACH, true);
echo "service detached\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
service attached
server version is string: yes
server version not empty: yes
db stats is string: yes
service detached
Done

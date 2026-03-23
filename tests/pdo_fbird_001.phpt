--TEST--
pdo_fbird: basic connection via fbird: DSN prefix
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

try {
    $pdo = pdo_fbird_connect();
    echo "Connected\n";
    $ver = $pdo->getAttribute(PDO::ATTR_SERVER_VERSION);
    echo "Server version: " . (strlen($ver) > 0 ? "ok" : "empty") . "\n";
    echo "Driver name: " . $pdo->getAttribute(PDO::ATTR_DRIVER_NAME) . "\n";
    $pdo = null;
    echo "Disconnected\n";
} catch (PDOException $e) {
    echo "FAIL: " . $e->getMessage() . "\n";
}
?>
--EXPECT--
Connected
Server version: ok
Driver name: fbird
Disconnected

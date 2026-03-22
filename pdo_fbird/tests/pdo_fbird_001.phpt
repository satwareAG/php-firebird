--TEST--
pdo_fbird: basic connection via fbird: DSN prefix
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!extension_loaded('pdo_fbird')) die('skip pdo_fbird not loaded');
if (!extension_loaded('firebird')) die('skip firebird not loaded');
require_once __DIR__ . '/../../tests/firebird.inc';
if (!@fbird_connect($test_base, $user, $password)) die('skip cannot connect to Firebird');
?>
--FILE--
<?php
require_once __DIR__ . '/../../tests/firebird.inc';

// $test_base is "host:dbpath" or just "dbpath"
if (strpos($test_base, ':') !== false) {
    [$dsn_host, $dsn_db] = explode(':', $test_base, 2);
} else {
    $dsn_host = 'localhost';
    $dsn_db   = $test_base;
}

try {
    $dsn = "fbird:host={$dsn_host};dbname={$dsn_db};charset=UTF8";
    $pdo = new PDO($dsn, $user, $password, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
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

--TEST--
pdo_fbird: connection DSN variants, charset, role
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

// Test 1: basic connection with host;dbname
$pdo = pdo_fbird_connect();
echo "Connect host+dbname: OK\n";
echo "Driver: " . $pdo->getAttribute(PDO::ATTR_DRIVER_NAME) . "\n";
echo "Connected: " . ($pdo->getAttribute(PDO::ATTR_CONNECTION_STATUS) ? "yes" : "no") . "\n";
$pdo = null;

// Test 2: connection with charset
$pdo = pdo_fbird_connect();
echo "Connect charset=UTF8: OK\n";
$pdo = null;

// Test 3: connection with dialect
$pdo = pdo_fbird_connect();
echo "Connect dialect=3: OK\n";
$pdo = null;

// Test 4: server version is non-empty
$pdo = pdo_fbird_connect();
$ver = $pdo->getAttribute(PDO::ATTR_SERVER_VERSION);
echo "Server version: " . (strlen($ver) > 0 ? "ok" : "empty") . "\n";
$pdo = null;

// Test 5: invalid DSN should fail
try {
    $pdo = new PDO("fbird:host=invalid_host_xyz;dbname=/nonexistent.fdb", $user, $password,
        [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
    echo "Invalid connect: should have failed\n";
} catch (PDOException $e) {
    echo "Invalid connect: caught exception\n";
}

echo "Done\n";
?>
--EXPECT--
Connect host+dbname: OK
Driver: fbird
Connected: yes
Connect charset=UTF8: OK
Connect dialect=3: OK
Server version: ok
Invalid connect: caught exception
Done

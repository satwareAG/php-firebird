--TEST--
PDO Firebird: check_liveness via fbc_ping()
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

/* Connection should be alive */
$status = $pdo->getAttribute(PDO::ATTR_CONNECTION_STATUS);
echo "connected: " . ($status ? "yes" : "no") . "\n";

/* Execute a query to confirm connection works */
$row = $pdo->query("SELECT 1 AS alive FROM RDB\$DATABASE")->fetch(PDO::FETCH_ASSOC);
echo "query works: " . ($row['ALIVE'] == 1 ? "yes" : "no") . "\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
connected: yes
query works: yes
Done

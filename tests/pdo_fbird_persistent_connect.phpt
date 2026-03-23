--TEST--
pdo_fbird: persistent connection attribute handling
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

/* Non-persistent connection */
$pdo1 = pdo_fbird_connect([PDO::ATTR_PERSISTENT => false]);
$stmt = $pdo1->query("SELECT 1 AS val FROM RDB\$DATABASE");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "non_persist: {$row['VAL']}\n";

/* Check persistent attribute */
$p = $pdo1->getAttribute(PDO::ATTR_PERSISTENT);
echo "attr: " . ($p ? "true" : "false") . "\n";
$pdo1 = null;

/* Multiple non-persistent connections */
$pdo2 = pdo_fbird_connect();
$pdo3 = pdo_fbird_connect();
$stmt2 = $pdo2->query("SELECT 2 AS val FROM RDB\$DATABASE");
$stmt3 = $pdo3->query("SELECT 3 AS val FROM RDB\$DATABASE");
echo "conn2: " . $stmt2->fetchColumn() . "\n";
echo "conn3: " . $stmt3->fetchColumn() . "\n";

$pdo2 = null;
$pdo3 = null;
echo "Done\n";
?>
--EXPECT--
non_persist: 1
attr: false
conn2: 2
conn3: 3
Done

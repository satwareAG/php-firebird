--TEST--
pdo_fbird: NULL binding on NOT NULL columns via sqltype |= 1
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

$pdo->exec("RECREATE TABLE pdo_null_test (name VARCHAR(50), val INTEGER NOT NULL)");

// Insert with NULL for nullable column
$stmt = $pdo->prepare("INSERT INTO pdo_null_test (name, val) VALUES (?, ?)");
$stmt->execute([null, 42]);
echo "Insert with NULL name: OK\n";

// Verify
$row = $pdo->query("SELECT name, val FROM pdo_null_test")->fetch(PDO::FETCH_ASSOC);
echo "name is NULL: " . (is_null($row['NAME']) ? "yes" : "no") . "\n";
echo "val: " . $row['VAL'] . "\n";

// Insert with values
$stmt->execute(['hello', 99]);
echo "Insert with values: OK\n";

$row = $pdo->query("SELECT name, val FROM pdo_null_test WHERE val = 99")->fetch(PDO::FETCH_ASSOC);
echo "name: " . trim($row['NAME']) . ", val: " . $row['VAL'] . "\n";

$pdo->exec("DROP TABLE pdo_null_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
Insert with NULL name: OK
name is NULL: yes
val: 42
Insert with values: OK
name: hello, val: 99
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
@$pdo->exec("DROP TABLE pdo_null_test");
unset($pdo);
?>

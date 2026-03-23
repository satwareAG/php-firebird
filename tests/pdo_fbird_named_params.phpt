--TEST--
pdo_fbird: named parameter :name resolution in prepared statements
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

$pdo->exec("RECREATE TABLE pdo_named_test (id INTEGER, name VARCHAR(50))");
$pdo->exec("INSERT INTO pdo_named_test (id, name) VALUES (1, 'alpha')");
$pdo->exec("INSERT INTO pdo_named_test (id, name) VALUES (2, 'beta')");

// Positional parameter (?) — always works
$stmt = $pdo->prepare("SELECT id, name FROM pdo_named_test WHERE id = ?");
$stmt->execute([1]);
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Row 1: " . trim($row['NAME']) . ", " . $row['ID'] . "\n";

$stmt->execute([2]);
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Row 2: " . trim($row['NAME']) . ", " . $row['ID'] . "\n";

// Multiple positional parameters
$stmt2 = $pdo->prepare("SELECT id, name FROM pdo_named_test WHERE id = ? AND name = ?");
$stmt2->execute([1, 'alpha']);
$row = $stmt2->fetch(PDO::FETCH_ASSOC);
echo "Multi: " . trim($row['NAME']) . ", " . $row['ID'] . "\n";

$pdo->exec("DROP TABLE pdo_named_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
Row 1: alpha, 1
Row 2: beta, 2
Multi: alpha, 1
Done

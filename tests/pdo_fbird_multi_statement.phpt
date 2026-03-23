--TEST--
pdo_fbird: multiple concurrent statements
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

$pdo->exec("RECREATE TABLE multi_test (id INTEGER, grp VARCHAR(10))");
$pdo->exec("INSERT INTO multi_test VALUES (1, 'A')");
$pdo->exec("INSERT INTO multi_test VALUES (2, 'A')");
$pdo->exec("INSERT INTO multi_test VALUES (3, 'B')");
$pdo->exec("INSERT INTO multi_test VALUES (4, 'B')");

/* Two statements open at the same time */
$stmt1 = $pdo->prepare("SELECT id FROM multi_test WHERE grp = ?");
$stmt2 = $pdo->prepare("SELECT id FROM multi_test WHERE grp = ?");

$stmt1->execute(['A']);
$stmt2->execute(['B']);

$r1 = $stmt1->fetchAll(PDO::FETCH_COLUMN);
$r2 = $stmt2->fetchAll(PDO::FETCH_COLUMN);

echo "grp_A: " . implode(',', $r1) . "\n";
echo "grp_B: " . implode(',', $r2) . "\n";

/* Re-execute stmt1 with different param */
$stmt1->execute(['B']);
$r3 = $stmt1->fetchAll(PDO::FETCH_COLUMN);
echo "reexec: " . implode(',', $r3) . "\n";

$pdo->exec("DROP TABLE multi_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
grp_A: 1,2
grp_B: 3,4
reexec: 3,4
Done

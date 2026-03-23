--TEST--
pdo_fbird: rowCount() accuracy after DML operations
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

$pdo->exec("RECREATE TABLE pdo_rc_test (id INTEGER, name VARCHAR(50))");
$pdo->exec("INSERT INTO pdo_rc_test VALUES (1, 'a')");
$pdo->exec("INSERT INTO pdo_rc_test VALUES (2, 'b')");
$pdo->exec("INSERT INTO pdo_rc_test VALUES (3, 'c')");

// rowCount after UPDATE
$count = $pdo->exec("UPDATE pdo_rc_test SET name = 'x' WHERE id <= 2");
echo "Update rowCount: $count\n";

// rowCount after DELETE
$count = $pdo->exec("DELETE FROM pdo_rc_test WHERE id = 3");
echo "Delete rowCount: $count\n";

$pdo->exec("DROP TABLE pdo_rc_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
Update rowCount: 2
Delete rowCount: 1
Done

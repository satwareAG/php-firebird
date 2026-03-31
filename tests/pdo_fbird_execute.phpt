--TEST--
pdo_fbird: INSERT/UPDATE/DELETE/SELECT execute variations
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

$pdo->exec("RECREATE TABLE pdo_exec_test (id INTEGER, name VARCHAR(50))");

// INSERT
$pdo->exec("INSERT INTO pdo_exec_test (id, name) VALUES (1, 'alpha')");
$pdo->exec("INSERT INTO pdo_exec_test (id, name) VALUES (2, 'beta')");
echo "Inserted 2 rows\n";

// SELECT
$stmt = $pdo->query("SELECT id, name FROM pdo_exec_test ORDER BY id");
while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
    echo "Row: " . $row['ID'] . ", " . trim($row['NAME']) . "\n";
}

// UPDATE
$count = $pdo->exec("UPDATE pdo_exec_test SET name = 'gamma' WHERE id = 1");
echo "Updated: $count\n";

// DELETE
$count = $pdo->exec("DELETE FROM pdo_exec_test WHERE id = 2");
echo "Deleted: $count\n";

// Verify
$stmt = $pdo->query("SELECT id, name FROM pdo_exec_test ORDER BY id");
while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
    echo "After: " . $row['ID'] . ", " . trim($row['NAME']) . "\n";
}

$pdo->exec("DROP TABLE pdo_exec_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
Inserted 2 rows
Row: 1, alpha
Row: 2, beta
Updated: 1
Deleted: 1
After: 1, gamma
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
@$pdo->exec("DROP TABLE pdo_exec_test");
unset($pdo);
?>

--TEST--
pdo_fbird: statement cleanup (closeCursor, re-execute, destructor)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

$pdo->exec("RECREATE TABLE cleanup_test (id INTEGER)");
$pdo->exec("INSERT INTO cleanup_test VALUES (1)");
$pdo->exec("INSERT INTO cleanup_test VALUES (2)");
$pdo->exec("INSERT INTO cleanup_test VALUES (3)");

/* closeCursor mid-fetch */
$stmt = $pdo->query("SELECT id FROM cleanup_test ORDER BY id");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "first: {$row['ID']}\n";
$stmt->closeCursor();
echo "cursor closed\n";

/* Re-execute after closeCursor */
$stmt = $pdo->prepare("SELECT id FROM cleanup_test WHERE id > ?");
$stmt->execute([0]);
$rows = $stmt->fetchAll(PDO::FETCH_COLUMN);
echo "exec1: " . implode(',', $rows) . "\n";

$stmt->closeCursor();
$stmt->execute([1]);
$rows = $stmt->fetchAll(PDO::FETCH_COLUMN);
echo "exec2: " . implode(',', $rows) . "\n";

/* Statement destructor — just let it go out of scope */
$stmt2 = $pdo->query("SELECT id FROM cleanup_test ORDER BY id");
$stmt2->fetch(PDO::FETCH_ASSOC); /* partial fetch */
$stmt2 = null; /* destructor should clean up */
echo "destructor ok\n";

/* Connection still works after stmt cleanup */
$stmt3 = $pdo->query("SELECT COUNT(*) AS cnt FROM cleanup_test");
$row = $stmt3->fetch(PDO::FETCH_ASSOC);
echo "count: {$row['CNT']}\n";

$pdo->exec("DROP TABLE cleanup_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
first: 1
cursor closed
exec1: 1,2,3
exec2: 2,3
destructor ok
count: 3
Done

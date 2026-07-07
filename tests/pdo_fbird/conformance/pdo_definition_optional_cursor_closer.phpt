--TEST--
PDO Definition: optional cursor_closer (closeCursor)
--CREDITS--
v12.1.0 M1-14 (#341)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Optional: cursor_closer ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_cursor (id INT)");
$pdo->exec("INSERT INTO test_cursor VALUES (1)");
$pdo->exec("INSERT INTO test_cursor VALUES (2)");
$stmt = $pdo->prepare("SELECT id FROM test_cursor WHERE id = ?");
$stmt->execute([1]);
$r1 = $stmt->fetchColumn();
$stmt->closeCursor();
$stmt->execute([2]);
$r2 = $stmt->fetchColumn();
echo "OK re-execute after closeCursor: id1=$r1 id2=$r2\n";
echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_cursor"); ?>
--EXPECT--
=== Optional: cursor_closer ===
OK re-execute after closeCursor: id1=1 id2=2
=== DONE ===

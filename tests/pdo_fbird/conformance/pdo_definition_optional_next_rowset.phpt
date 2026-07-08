--TEST--
PDO Definition: optional next_rowset (documented gap)
--CREDITS--
v12.1.0 M1-13 (#340)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Optional: next_rowset ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_rowset (id INT)");
$pdo->exec("INSERT INTO test_rowset VALUES (1)");
$stmt = $pdo->query("SELECT id FROM test_rowset");
$stmt->fetch();
$r = $stmt->nextRowset();
echo "OK nextRowset: " . var_export($r, true) . " (false = not supported, documented gap)\n";
echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_rowset"); ?>
--EXPECT--
=== Optional: next_rowset ===
OK nextRowset: false (false = not supported, documented gap)
=== DONE ===

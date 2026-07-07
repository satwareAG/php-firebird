--TEST--
PDO Definition: optional param_hook (bind param events)
--CREDITS--
v12.1.0 M1-11 (#338)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Optional: param_hook ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_param_hook (id INT, name VARCHAR(50))");

// bindValue - value copied at bind time
$stmt = $pdo->prepare("INSERT INTO test_param_hook VALUES (?, ?)");
$stmt->bindValue(1, 100);
$stmt->bindValue(2, 'bound_value');
$val = 999;
$stmt->bindParam(1, $val); // by ref - will use current value at execute
$val = 42;
$stmt->execute();
echo "OK bindParam by-ref: " . $pdo->query("SELECT id FROM test_param_hook")->fetchColumn() . "\n";

// Re-execute with changed ref
$val = 99;
$stmt->execute();
$rows = $pdo->query("SELECT id FROM test_param_hook ORDER BY id")->fetchAll(PDO::FETCH_COLUMN);
echo "OK re-execute with new ref: " . implode(',', $rows) . "\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_param_hook"); ?>
--EXPECTF--
=== Optional: param_hook ===
OK bindParam by-ref: 42
OK re-execute with new ref: 42,99
=== DONE ===

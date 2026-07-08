--TEST--
PDO Definition: null handling + ATTR_ORACLE_NULLS
--CREDITS--
v12.1.0 M1-20 (#347)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Null handling ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_nulls (id INT, val VARCHAR(50))");
$pdo->exec("INSERT INTO test_nulls VALUES (1, NULL)");
$pdo->exec("INSERT INTO test_nulls VALUES (2, 'hello')");
$pdo->exec("INSERT INTO test_nulls VALUES (3, '')");

// NULL_NATURAL (default): NULL stays NULL, empty string stays empty
$pdo->setAttribute(PDO::ATTR_ORACLE_NULLS, PDO::NULL_NATURAL);
$stmt = $pdo->query("SELECT val FROM test_nulls WHERE id = 1");
$r = $stmt->fetchColumn();
echo "OK NULL_NATURAL (null): " . var_export($r, true) . "\n";

$stmt = $pdo->query("SELECT val FROM test_nulls WHERE id = 3");
$r = $stmt->fetchColumn();
echo "OK NULL_NATURAL (empty): " . var_export($r, true) . "\n";

// NULL_EMPTY_STRING: empty string becomes NULL
$pdo->setAttribute(PDO::ATTR_ORACLE_NULLS, PDO::NULL_EMPTY_STRING);
$stmt = $pdo->query("SELECT val FROM test_nulls WHERE id = 3");
$r = $stmt->fetchColumn();
echo "OK NULL_EMPTY_STRING (empty->null): " . var_export($r, true) . "\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_nulls"); ?>
--EXPECTF--
=== Null handling ===
OK NULL_NATURAL (null): NULL
OK NULL_NATURAL (empty): %s
OK NULL_EMPTY_STRING (empty->null): %s
=== DONE ===

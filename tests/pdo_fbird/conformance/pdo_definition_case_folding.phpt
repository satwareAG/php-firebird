--TEST--
PDO Definition: case folding (CASE_NATURAL/LOWER/UPPER)
--CREDITS--
v12.1.0 M1-19 (#346)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Case folding ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_case (ID INT, NAME VARCHAR(50))");
$pdo->exec("INSERT INTO test_case VALUES (1, 'hello')");

// CASE_NATURAL (FB default: uppercase)
$pdo->setAttribute(PDO::ATTR_CASE, PDO::CASE_NATURAL);
$r = $pdo->query("SELECT * FROM test_case")->fetch(PDO::FETCH_ASSOC);
$keys = array_keys($r);
echo "OK CASE_NATURAL: " . $keys[0] . "\n";

// CASE_LOWER
$pdo->setAttribute(PDO::ATTR_CASE, PDO::CASE_LOWER);
$r = $pdo->query("SELECT * FROM test_case")->fetch(PDO::FETCH_ASSOC);
$keys = array_keys($r);
echo "OK CASE_LOWER: " . $keys[0] . "\n";

// CASE_UPPER
$pdo->setAttribute(PDO::ATTR_CASE, PDO::CASE_UPPER);
$r = $pdo->query("SELECT * FROM test_case")->fetch(PDO::FETCH_ASSOC);
$keys = array_keys($r);
echo "OK CASE_UPPER: " . $keys[0] . "\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_case"); ?>
--EXPECT--
=== Case folding ===
OK CASE_NATURAL: ID
OK CASE_LOWER: id
OK CASE_UPPER: ID
=== DONE ===

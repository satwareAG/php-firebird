--TEST--
PDO Definition: all standard PARAM_* types
--CREDITS--
v12.1.0 M1-24 (#351)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== PARAM_* types ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_params (id INT, str VARCHAR(100), flag BOOLEAN, note BLOB SUB_TYPE TEXT)");
$pdo->exec("INSERT INTO test_params (id, str, flag) VALUES (1, 'init', true)");

// PARAM_STR
$stmt = $pdo->prepare("UPDATE test_params SET str = ? WHERE id = 1");
$stmt->bindValue(1, 'string_val', PDO::PARAM_STR);
$stmt->execute();
echo "OK PARAM_STR: " . $pdo->query("SELECT str FROM test_params WHERE id = 1")->fetchColumn() . "\n";

// PARAM_INT
$stmt = $pdo->prepare("UPDATE test_params SET id = ? WHERE id = 1");
$stmt->bindValue(1, 55, PDO::PARAM_INT);
$stmt->execute();
echo "OK PARAM_INT: " . $pdo->query("SELECT id FROM test_params WHERE str = 'string_val'")->fetchColumn() . "\n";

// PARAM_NULL
$stmt = $pdo->prepare("UPDATE test_params SET str = ? WHERE id = 55");
$stmt->bindValue(1, null, PDO::PARAM_NULL);
$stmt->execute();
$r = $pdo->query("SELECT str FROM test_params WHERE id = 55")->fetchColumn();
echo "OK PARAM_NULL: " . var_export($r, true) . "\n";

// PARAM_BOOL
$stmt = $pdo->prepare("UPDATE test_params SET flag = ? WHERE id = 55");
$stmt->bindValue(1, false, PDO::PARAM_BOOL);
$stmt->execute();
$r = $pdo->query("SELECT flag FROM test_params WHERE id = 55")->fetchColumn();
echo "OK PARAM_BOOL: " . var_export($r, true) . "\n";

// PARAM_LOB (BLOB read - bindColumn with PARAM_LOB)
$pdo->exec("RECREATE TABLE test_lob (id INT, data BLOB SUB_TYPE TEXT)");
$pdo->exec("INSERT INTO test_lob (id, data) VALUES (1, 'blob content here')");
$stmt = $pdo->prepare("SELECT data FROM test_lob WHERE id = 1");
$stmt->execute();
$stmt->bindColumn(1, $lob, PDO::PARAM_LOB);
$stmt->fetch(PDO::FETCH_BOUND);
$r = is_resource($lob) ? stream_get_contents($lob) : $lob;
echo "OK PARAM_LOB (read): " . (strlen((string)$r) > 0 ? strlen($r) . ' bytes' : 'empty') . "\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_params"); @$pdo->exec("DROP TABLE test_lob"); ?>
--EXPECTF--
=== PARAM_* types ===
OK PARAM_STR: string_val
OK PARAM_INT: 55
OK PARAM_NULL: NULL
OK PARAM_BOOL: %s
OK PARAM_LOB (read): %s bytes
=== DONE ===

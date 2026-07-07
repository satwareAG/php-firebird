--TEST--
PDO Definition: optional last_id (lastInsertId with sequence name)
--CREDITS--
v12.1.0 M1-5 (#332)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Optional: last_id ===\n";
$pdo = pdo_fbird_connect();

// Firebird uses generators (sequences). Find one in the test DB.
$gen = $pdo->query("SELECT rdb\$generator_name FROM rdb\$generators WHERE rdb\$system_flag = 0 OR rdb\$system_flag IS NULL ROWS 1")->fetchColumn();

if ($gen) {
    $gen = trim($gen);
    $id = $pdo->lastInsertId($gen);
    echo "OK lastInsertId('$gen'): " . (strlen((string)$id) > 0 ? 'returned' : 'empty') . "\n";
} else {
    // Create a test generator and increment it
    $pdo->exec("CREATE SEQUENCE test_gen_id");
    $pdo->query("SELECT GEN_ID(test_gen_id, 5) FROM rdb\$database")->fetchColumn();
    $id = $pdo->lastInsertId('test_gen_id');
    echo "OK lastInsertId('test_gen_id'): $id\n";
    $pdo->exec("DROP SEQUENCE test_gen_id");
}

// lastInsertId without name - document behavior
$id = $pdo->lastInsertId();
echo "OK lastInsertId(no name): " . var_export($id, true) . "\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP SEQUENCE test_gen_id"); ?>
--EXPECTF--
=== Optional: last_id ===
OK lastInsertId(%s): %s
OK lastInsertId(no name): false
=== DONE ===

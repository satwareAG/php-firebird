--TEST--
PDO Definition: all standard FETCH modes
--CREDITS--
v12.1.0 M1-15 (#342)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== All FETCH modes ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_fetch (id INT, name VARCHAR(50))");
$pdo->exec("INSERT INTO test_fetch VALUES (1, 'alpha')");
$pdo->exec("INSERT INTO test_fetch VALUES (2, 'beta')");

function get_row($pdo) {
    return $pdo->query("SELECT id, name FROM test_fetch WHERE id = 1");
}

// FETCH_ASSOC
$r = get_row($pdo)->fetch(PDO::FETCH_ASSOC);
echo "OK FETCH_ASSOC: " . (isset($r['NAME']) ? 'yes' : 'no') . "\n";

// FETCH_NUM
$r = get_row($pdo)->fetch(PDO::FETCH_NUM);
echo "OK FETCH_NUM: " . (isset($r[0]) ? 'yes' : 'no') . "\n";

// FETCH_BOTH
$r = get_row($pdo)->fetch(PDO::FETCH_BOTH);
echo "OK FETCH_BOTH: " . (isset($r[0]) && isset($r['NAME']) ? 'yes' : 'no') . "\n";

// FETCH_OBJ
$r = get_row($pdo)->fetch(PDO::FETCH_OBJ);
echo "OK FETCH_OBJ: " . (isset($r->NAME) ? 'yes' : 'no') . "\n";

// FETCH_COLUMN
$r = get_row($pdo)->fetchColumn(1);
echo "OK FETCH_COLUMN: $r\n";

// FETCH_KEY_PAIR
$stmt = $pdo->query("SELECT id, name FROM test_fetch ORDER BY id");
$r = $stmt->fetchAll(PDO::FETCH_KEY_PAIR);
echo "OK FETCH_KEY_PAIR: " . count($r) . " pairs\n";

// FETCH_BOUND
$stmt = get_row($pdo);
$id = 0; $name = '';
$stmt->bindColumn(1, $id);
$stmt->bindColumn(2, $name);
$stmt->fetch(PDO::FETCH_BOUND);
echo "OK FETCH_BOUND: id=$id\n";

// FETCH_LAZY - returns PDORow object
$r = get_row($pdo)->fetch(PDO::FETCH_LAZY);
echo "OK FETCH_LAZY: " . ($r instanceof \PDORow ? 'PDORow' : get_class($r)) . "\n";

// FETCH_CLASS
class TestFetchClass {
    public $ID;
    public $NAME;
}
// FETCH_CLASS - use setFetchMode + fetch (not positional args)
$stmt = get_row($pdo);
$stmt->setFetchMode(PDO::FETCH_CLASS, 'TestFetchClass');
$r = $stmt->fetch();
echo "OK FETCH_CLASS: " . ($r instanceof TestFetchClass ? 'yes' : 'no') . "\n";

// FETCH_INTO
$obj = new stdClass();
$stmt = get_row($pdo);
$stmt->setFetchMode(PDO::FETCH_INTO, $obj);
$stmt->fetch();
echo "OK FETCH_INTO: " . (isset($obj->NAME) ? 'yes' : 'no') . "\n";

// FETCH_NAMED (like ASSOC but groups duplicate columns)
$r = get_row($pdo)->fetch(PDO::FETCH_NAMED);
echo "OK FETCH_NAMED: " . (isset($r['NAME']) ? 'yes' : 'no') . "\n";

// FETCH_FUNC - only works with fetchAll per PHP spec
$stmt = $pdo->query("SELECT id, name FROM test_fetch WHERE id = 1");
$r = $stmt->fetchAll(PDO::FETCH_FUNC, function($id, $name) { return "$id:$name"; });
echo "OK FETCH_FUNC: " . $r[0] . "\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_fetch"); ?>
--EXPECTF--
=== All FETCH modes ===
OK FETCH_ASSOC: yes
OK FETCH_NUM: yes
OK FETCH_BOTH: yes
OK FETCH_OBJ: yes
OK FETCH_COLUMN: alpha
OK FETCH_KEY_PAIR: 2 pairs
OK FETCH_BOUND: id=1
OK FETCH_LAZY: PDORow
OK FETCH_CLASS: yes
OK FETCH_INTO: yes
OK FETCH_NAMED: yes
OK FETCH_FUNC: 1:alpha
=== DONE ===

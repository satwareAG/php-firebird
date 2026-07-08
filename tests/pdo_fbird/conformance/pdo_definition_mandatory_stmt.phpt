--TEST--
PDO Definition: mandatory stmt methods (executer/fetcher/describer/get_col)
--CREDITS--
v12.1.0 M1-3 (#330) - PDO Definition conformance
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Mandatory Stmt: executer/fetcher/describer/get_col ===\n";

$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_mandatory_stmt (id INT, name VARCHAR(50))");
$pdo->exec("INSERT INTO test_mandatory_stmt VALUES (1, 'alpha')");
$pdo->exec("INSERT INTO test_mandatory_stmt VALUES (2, 'beta')");
$pdo->exec("INSERT INTO test_mandatory_stmt VALUES (3, 'gamma')");

// 1. preparer + executer
$stmt = $pdo->prepare("SELECT id, name FROM test_mandatory_stmt ORDER BY id");
$stmt->execute();
echo "OK execute\n";

// 2. describer — columnCount() must return int
$cols = $stmt->columnCount();
if ($cols === 2) {
    echo "OK columnCount: $cols\n";
} else {
    echo "FAIL columnCount: expected 2, got " . var_export($cols, true) . "\n";
}

// 3. fetcher — fetch() returns array (ASSOC mode)
$row = $stmt->fetch(PDO::FETCH_ASSOC);
if (is_array($row) && $row['ID'] === 1 && $row['NAME'] === 'alpha') {
    echo "OK fetch[0]: id={$row['ID']} name={$row['NAME']}\n";
} else {
    echo "FAIL fetch[0]: " . var_export($row, true) . "\n";
}

// 4. fetcher — subsequent fetch() iterates
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "OK fetch[1]: id={$row['ID']} name={$row['NAME']}\n";

$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "OK fetch[2]: id={$row['ID']} name={$row['NAME']}\n";

// 5. fetcher — past end returns false
$row = $stmt->fetch(PDO::FETCH_ASSOC);
if ($row === false) {
    echo "OK fetch[3] past end: false\n";
} else {
    echo "FAIL fetch[3] past end: expected false, got " . gettype($row) . "\n";
}

// 6. get_col — fetch numeric mode returns correct column values
$stmt = $pdo->prepare("SELECT id, name FROM test_mandatory_stmt ORDER BY id");
$stmt->execute();
$ids = [];
while ($row = $stmt->fetch(PDO::FETCH_NUM)) {
    $ids[] = $row[0];  // get_col for column 0
}
// IDs may be int or string depending on driver type coercion
$expected_ids = [1, 2, 3];
$ids_match = count($ids) === 3
    && (int)$ids[0] === $expected_ids[0]
    && (int)$ids[1] === $expected_ids[1]
    && (int)$ids[2] === $expected_ids[2];
if ($ids_match) {
    echo "OK get_col iteration: ids=" . implode(',', $ids) . "\n";
} else {
    echo "FAIL get_col iteration: " . var_export($ids, true) . "\n";
}

// 7. fetchAll — bulk fetch via fetcher+get_col
$stmt = $pdo->prepare("SELECT name FROM test_mandatory_stmt ORDER BY id");
$stmt->execute();
$names = $stmt->fetchAll(PDO::FETCH_COLUMN);
echo "OK fetchAll(COLUMN): " . implode(',', $names) . "\n";

// 8. closeCursor — allows re-execute
$stmt = $pdo->prepare("SELECT id FROM test_mandatory_stmt WHERE id = ?");
$stmt->execute([1]);
$r1 = $stmt->fetchColumn();
$stmt->closeCursor();
$stmt->execute([2]);
$r2 = $stmt->fetchColumn();
echo "OK re-execute after closeCursor: id1=$r1 id2=$r2\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
try {
    $pdo = pdo_fbird_connect();
    @$pdo->exec("DROP TABLE test_mandatory_stmt");
} catch (Throwable $e) {
    // ignore
}
?>
--EXPECT--
=== Mandatory Stmt: executer/fetcher/describer/get_col ===
OK execute
OK columnCount: 2
OK fetch[0]: id=1 name=alpha
OK fetch[1]: id=2 name=beta
OK fetch[2]: id=3 name=gamma
OK fetch[3] past end: false
OK get_col iteration: ids=1,2,3
OK fetchAll(COLUMN): alpha,beta,gamma
OK re-execute after closeCursor: id1=1 id2=2
=== DONE ===

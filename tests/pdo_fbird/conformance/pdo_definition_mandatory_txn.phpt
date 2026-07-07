--TEST--
PDO Definition: mandatory transaction methods (begin/commit/rollBack)
--CREDITS--
v12.1.0 M1-2 (#329) - PDO Definition conformance
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Mandatory Transaction: begin/commit/rollBack ===\n";

$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_mandatory_txn (id INT, val VARCHAR(50))");

// 1. beginTransaction + commit
$pdo->beginTransaction();
echo "OK beginTransaction\n";

$r = $pdo->inTransaction();
echo "OK inTransaction (inside): " . ($r ? 'true' : 'false') . "\n";

$pdo->exec("INSERT INTO test_mandatory_txn (id, val) VALUES (1, 'committed')");
$pdo->commit();
echo "OK commit\n";

$r = $pdo->inTransaction();
echo "OK inTransaction (after commit): " . ($r ? 'true' : 'false') . "\n";

// Verify row persisted
$count = (int)$pdo->query("SELECT COUNT(*) FROM test_mandatory_txn WHERE id = 1")->fetchColumn();
echo "OK committed row visible: count=$count\n";

// 2. beginTransaction + rollBack
$pdo->beginTransaction();
$pdo->exec("INSERT INTO test_mandatory_txn (id, val) VALUES (2, 'rolled-back')");
$pdo->rollBack();
echo "OK rollBack\n";

// Verify row NOT persisted
$count = (int)$pdo->query("SELECT COUNT(*) FROM test_mandatory_txn WHERE id = 2")->fetchColumn();
echo "OK rolled-back row not visible: count=$count\n";

// 3. beginTransaction must throw if already in transaction (ERRMODE_EXCEPTION)
$pdo->beginTransaction();
try {
    $pdo->beginTransaction();
    echo "FAIL nested beginTransaction should throw\n";
} catch (PDOException $e) {
    echo "OK nested beginTransaction throws\n";
}
$pdo->rollBack();

echo "=== DONE ===\n";
?>
--CLEAN--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
try {
    $pdo = pdo_fbird_connect();
    @$pdo->exec("DROP TABLE test_mandatory_txn");
} catch (Throwable $e) {
    // ignore
}
?>
--EXPECT--
=== Mandatory Transaction: begin/commit/rollBack ===
OK beginTransaction
OK inTransaction (inside): true
OK commit
OK inTransaction (after commit): false
OK committed row visible: count=1
OK rollBack
OK rolled-back row not visible: count=0
OK nested beginTransaction throws
=== DONE ===

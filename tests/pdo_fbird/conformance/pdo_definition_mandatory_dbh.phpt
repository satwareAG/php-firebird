--TEST--
PDO Definition: mandatory DBH methods (closer/preparer/doer)
--CREDITS--
v12.1.0 M1-1 (#328) - PDO Definition conformance
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Mandatory DBH: closer/preparer/doer ===\n";

// 1. Constructor (db_handle_factory) — must succeed
$pdo = pdo_fbird_connect();
echo "OK construct: " . get_class($pdo) . "\n";

// 2. preparer — prepare() must return PDOStatement
$stmt = $pdo->prepare("SELECT 1 AS VAL FROM rdb\$database");
if ($stmt instanceof PDOStatement) {
    echo "OK prepare: PDOStatement\n";
} else {
    echo "FAIL prepare: expected PDOStatement, got " . gettype($stmt) . "\n";
}

// 3. doer — exec() must return int (affected rows) for DDL
$pdo->exec("RECREATE TABLE test_mandatory_dbh (id INT, val VARCHAR(50))");
echo "OK exec DDL: RECREATE TABLE\n";

// exec() for DML must return affected row count
$affected = $pdo->exec("INSERT INTO test_mandatory_dbh (id, val) VALUES (1, 'hello')");
if ($affected === 1) {
    echo "OK exec DML: affected=$affected\n";
} else {
    echo "FAIL exec DML: expected 1, got " . var_export($affected, true) . "\n";
}

$affected = $pdo->exec("DELETE FROM test_mandatory_dbh WHERE id = 1");
if ($affected === 1) {
    echo "OK exec DELETE: affected=$affected\n";
} else {
    echo "FAIL exec DELETE: expected 1, got " . var_export($affected, true) . "\n";
}

// 4. closer — unset triggers dtor, must not error
unset($pdo);
echo "OK destruct (closer)\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
try {
    $pdo = pdo_fbird_connect();
    @$pdo->exec("DROP TABLE test_mandatory_dbh");
} catch (Throwable $e) {
    // ignore
}
?>
--EXPECT--
=== Mandatory DBH: closer/preparer/doer ===
OK construct: PDO
OK prepare: PDOStatement
OK exec DDL: RECREATE TABLE
OK exec DML: affected=1
OK exec DELETE: affected=1
OK destruct (closer)
=== DONE ===

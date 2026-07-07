--TEST--
PDO Definition: optional get_column_meta (IM001 gap documented)
--CREDITS--
v12.1.0 M1-12 (#339)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Optional: get_column_meta ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_col_meta (id INT, name VARCHAR(50))");
$pdo->exec("INSERT INTO test_col_meta VALUES (1, 'test')");
$stmt = $pdo->query("SELECT id, name FROM test_col_meta");
try {
    $meta = $stmt->getColumnMeta(0);
    echo "OK getColumnMeta: " . var_export($meta, true) . "\n";
} catch (\PDOException $e) {
    if ($e->getCode() === 'IM001') {
        echo "DOCUMENTED GAP: getColumnMeta returns IM001 (not implemented)\n";
        echo "See issue #339: stubs claim supported but driver returns IM001\n";
    } else {
        echo "UNEXPECTED error: " . $e->getMessage() . "\n";
    }
}
echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_col_meta"); ?>
--EXPECT--
=== Optional: get_column_meta ===
DOCUMENTED GAP: getColumnMeta returns IM001 (not implemented)
See issue #339: stubs claim supported but driver returns IM001
=== DONE ===

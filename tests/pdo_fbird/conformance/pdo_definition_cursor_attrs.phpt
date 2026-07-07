--TEST--
PDO Definition: standard cursor attrs
--CREDITS--
v12.1.0 M1-17 (#344)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== Cursor attrs ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_curattr (id INT)");
$pdo->exec("INSERT INTO test_curattr VALUES (1)");
$pdo->exec("INSERT INTO test_curattr VALUES (2)");

// Forward-only (default)
$stmt = $pdo->prepare("SELECT id FROM test_curattr ORDER BY id", [PDO::ATTR_CURSOR => PDO::CURSOR_FWDONLY]);
$stmt->execute();
$r1 = $stmt->fetchColumn();
$r2 = $stmt->fetchColumn();
echo "OK CURSOR_FWDONLY: $r1,$r2\n";

// Scroll (FB5+ only; on FB3 this may error or silently downgrade)
try {
    $stmt = $pdo->prepare("SELECT id FROM test_curattr ORDER BY id", [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]);
    $stmt->execute();
    $r1 = $stmt->fetchColumn();
    echo "OK CURSOR_SCROLL: first=$r1\n";
} catch (\PDOException $e) {
    echo "SKIP CURSOR_SCROLL (FB5+ only): " . $e->getCode() . "\n";
}

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_curattr"); ?>
--EXPECTF--
=== Cursor attrs ===
OK CURSOR_FWDONLY: 1,2
%s
=== DONE ===

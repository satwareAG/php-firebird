--TEST--
PDO Definition: FETCH_ORI_* orientations (FB5+ scrollable cursors)
--CREDITS--
v12.1.0 M1-16 (#343)
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
// Probe for scrollable cursor support instead of loading firebird.inc
// (firebird.inc creates a DB + registers shutdown, contradicting PDO test isolation)
require_once __DIR__ . '/../../pdo_fbird.inc';
try {
    $pdo = pdo_fbird_connect();
    $pdo->exec("RECREATE TABLE test_skip_scroll (id INT)");
    $pdo->exec("INSERT INTO test_skip_scroll VALUES (1)");
    $stmt = $pdo->prepare("SELECT id FROM test_skip_scroll",
        [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]);
    $stmt->execute();
    $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT);
    @$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_FIRST);
} catch (\Throwable $e) {
    die('skip scrollable cursors not supported (requires Firebird 5.0+): ' . $e->getMessage());
}
?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== FETCH_ORI orientations (FB5+) ===\n";
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE test_scroll (id INT, name VARCHAR(50))");
for ($i = 1; $i <= 5; $i++) {
    $pdo->exec("INSERT INTO test_scroll VALUES ($i, 'row$i')");
}

$stmt = $pdo->prepare("SELECT id, name FROM test_scroll ORDER BY id", [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]);
$stmt->execute();

// FETCH_ORI_NEXT
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT);
echo "OK NEXT: id=" . $r[0] . "\n";

// FETCH_ORI_FIRST
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_FIRST);
echo "OK FIRST: id=" . $r[0] . "\n";

// FETCH_ORI_LAST
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_LAST);
echo "OK LAST: id=" . $r[0] . "\n";

// FETCH_ORI_ABS (absolute position)
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_ABS, 3);
echo "OK ABS(3): id=" . $r[0] . "\n";

// FETCH_ORI_PRIOR
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_PRIOR);
echo "OK PRIOR: id=" . $r[0] . "\n";

// FETCH_ORI_REL (relative offset)
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_REL, 1);
echo "OK REL(1): id=" . $r[0] . "\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]); @$pdo->exec("DROP TABLE test_scroll"); ?>
--EXPECTF--
=== FETCH_ORI orientations (FB5+) ===
%s
=== DONE ===

--TEST--
pdo_fbird: scrollable cursor fetch orientations
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
require_once __DIR__ . '/pdo_fbird.inc';
try {
    $pdo = pdo_fbird_connect();
    $pdo->exec("RECREATE TABLE scroll_skip_test (id INTEGER)");
    $pdo->exec("INSERT INTO scroll_skip_test VALUES (1)");
    $pdo->exec("INSERT INTO scroll_skip_test VALUES (2)");
    $stmt = $pdo->prepare("SELECT id FROM scroll_skip_test ORDER BY id",
        [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]);
    $stmt->execute();
    $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_NEXT);
    $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_PRIOR);
    $stmt->closeCursor();
    $pdo->exec("DROP TABLE scroll_skip_test");
} catch (Throwable $e) {
    if (strpos($e->getMessage(), 'not supported') !== false) {
        die('skip scrollable cursors not supported (requires Firebird 5.0+ client and server)');
    }
}
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';

$pdo = pdo_fbird_connect();

$pdo->exec("RECREATE TABLE scroll_test (id INTEGER, val VARCHAR(20))");
$pdo->exec("INSERT INTO scroll_test VALUES (1, 'alpha')");
$pdo->exec("INSERT INTO scroll_test VALUES (2, 'beta')");
$pdo->exec("INSERT INTO scroll_test VALUES (3, 'gamma')");
$pdo->exec("INSERT INTO scroll_test VALUES (4, 'delta')");
$pdo->exec("INSERT INTO scroll_test VALUES (5, 'epsilon')");

/* Open scrollable cursor */
$stmt = $pdo->prepare(
    "SELECT id, val FROM scroll_test ORDER BY id",
    [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]
);
$stmt->execute();

/* Verify cursor type attribute */
$cursor = $stmt->getAttribute(PDO::ATTR_CURSOR);
echo "cursor_type: " . ($cursor == PDO::CURSOR_SCROLL ? "scroll" : "fwdonly") . "\n";

/* NEXT → row 1 */
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_NEXT);
echo "next: {$row['ID']}-{$row['VAL']}\n";

/* NEXT → row 2 */
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_NEXT);
echo "next: {$row['ID']}-{$row['VAL']}\n";

/* PRIOR → row 1 */
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_PRIOR);
echo "prior: {$row['ID']}-{$row['VAL']}\n";

/* FIRST → row 1 */
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_FIRST);
echo "first: {$row['ID']}-{$row['VAL']}\n";

/* LAST → row 5 */
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_LAST);
echo "last: {$row['ID']}-{$row['VAL']}\n";

/* ABS position 3 → row 3 (1-based) */
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_ABS, 3);
echo "abs(3): {$row['ID']}-{$row['VAL']}\n";

/* REL -1 → row 2 */
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_REL, -1);
echo "rel(-1): {$row['ID']}-{$row['VAL']}\n";

/* REL +2 → row 4 */
$row = $stmt->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_REL, 2);
echo "rel(+2): {$row['ID']}-{$row['VAL']}\n";

$stmt->closeCursor();

/* Verify forward-only cursor does NOT get scroll type */
$stmt2 = $pdo->prepare("SELECT id FROM scroll_test ORDER BY id");
$cursor2 = $stmt2->getAttribute(PDO::ATTR_CURSOR);
echo "default_cursor: " . ($cursor2 == PDO::CURSOR_FWDONLY ? "fwdonly" : "scroll") . "\n";

$pdo->exec("DROP TABLE scroll_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
cursor_type: scroll
next: 1-alpha
next: 2-beta
prior: 1-alpha
first: 1-alpha
last: 5-epsilon
abs(3): 3-gamma
rel(-1): 2-beta
rel(+2): 4-delta
default_cursor: fwdonly
Done

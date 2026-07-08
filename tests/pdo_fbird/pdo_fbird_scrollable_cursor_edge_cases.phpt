--TEST--
pdo_fbird: FB5+ scrollable cursor BOF/EOF and edge cases (#426)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/../fb_version_probe.inc';
if (!fb_server_supports('SCROLLABLE_CURSORS')) die('skip scrollable cursors not supported (requires FB5+)');
?>
--FILE--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';

$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

/* ============================================================
 * Issue #426: FB5 scrollable cursors — BOF/EOF detection + edge cases
 *
 * Tests the behavior fix: scrollable cursors must remain open after
 * BOF/EOF so the user can reposition. Previously the cursor was closed
 * on any 0 return, making repositioning impossible.
 * ============================================================ */

echo "=== Test 1: EOF detection — NEXT past last row ===\n";
$pdo->exec("RECREATE TABLE scroll_edge (id INT, val VARCHAR(10))");
for ($i = 1; $i <= 3; $i++) {
    $pdo->exec("INSERT INTO scroll_edge VALUES ($i, 'r$i')");
}
$stmt = $pdo->prepare("SELECT id FROM scroll_edge ORDER BY id", [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]);
$stmt->execute();
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT); /* row 1 */
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT); /* row 2 */
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT); /* row 3 */
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT); /* EOF */
echo "EOF returns false: " . ($r === false ? 'yes' : 'no') . "\n";

echo "\n=== Test 2: BOF detection — PRIOR before first row ===\n";
$stmt->closeCursor();
$stmt->execute();
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT); /* row 1 */
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_PRIOR); /* BOF */
echo "BOF returns false: " . ($r === false ? 'yes' : 'no') . "\n";

echo "\n=== Test 3: Cursor usable after EOF — reposition to LAST ===\n";
$stmt->closeCursor();
$stmt->execute();
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_LAST);  /* row 3 */
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT);   /* EOF */
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_LAST); /* reposition */
echo "Usable after EOF: " . ($r !== false ? "yes, id=" . $r[0] : 'no') . "\n";

echo "\n=== Test 4: Cursor usable after BOF — reposition to FIRST ===\n";
$stmt->closeCursor();
$stmt->execute();
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT);   /* row 1 */
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_PRIOR);   /* BOF */
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_FIRST); /* reposition */
echo "Usable after BOF: " . ($r !== false ? "yes, id=" . $r[0] : 'no') . "\n";

echo "\n=== Test 5: Empty result set ===\n";
$pdo->exec("RECREATE TABLE scroll_empty (id INT)");
$stmt2 = $pdo->prepare("SELECT id FROM scroll_empty", [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]);
$stmt2->execute();
$r = $stmt2->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT);
echo "Empty NEXT: " . ($r === false ? 'false (correct)' : 'unexpected row') . "\n";
$r = $stmt2->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_FIRST);
echo "Empty FIRST: " . ($r === false ? 'false (correct)' : 'unexpected row') . "\n";
$r = $stmt2->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_LAST);
echo "Empty LAST: " . ($r === false ? 'false (correct)' : 'unexpected row') . "\n";
$stmt2->closeCursor();

echo "\n=== Test 6: Single row — FIRST==LAST==ABS(1) ===\n";
$pdo->exec("RECREATE TABLE scroll_one (id INT)");
$pdo->exec("INSERT INTO scroll_one VALUES (42)");
$stmt3 = $pdo->prepare("SELECT id FROM scroll_one", [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]);
$stmt3->execute();
$r = $stmt3->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_FIRST);
echo "Single FIRST: id=" . $r[0] . "\n";
$r = $stmt3->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_LAST);
echo "Single LAST: id=" . $r[0] . "\n";
$r = $stmt3->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_ABS, 1);
echo "Single ABS(1): id=" . $r[0] . "\n";
$stmt3->closeCursor();

echo "\n=== Test 7: Out-of-bounds ABS ===\n";
$stmt->closeCursor();
$stmt->execute();
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_ABS, 0);
echo "ABS(0): " . ($r === false ? 'false (correct)' : 'unexpected row') . "\n";
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_ABS, 99);
echo "ABS(99): " . ($r === false ? 'false (correct)' : 'unexpected row') . "\n";
/* Cursor still usable after out-of-bounds */
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_FIRST);
echo "After OOB, FIRST: " . ($r !== false ? "yes, id=" . $r[0] : 'no') . "\n";

echo "\n=== Test 8: REL beyond boundaries ===\n";
$stmt->closeCursor();
$stmt->execute();
$stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT); /* row 1 */
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_REL, -99);
echo "REL(-99) from row 1: " . ($r === false ? 'false (BOF, correct)' : 'unexpected row') . "\n";
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_REL, 99);
echo "REL(+99) from BOF: " . ($r === false ? 'false (EOF, correct)' : 'unexpected row') . "\n";
/* Cursor still usable */
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_ABS, 2);
echo "After REL OOB, ABS(2): " . ($r !== false ? "yes, id=" . $r[0] : 'no') . "\n";

echo "\n=== Test 9: Cross-orientation navigation ===\n";
$stmt->closeCursor();
$stmt->execute();
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT);     /* 1 */
echo "NEXT: " . $r[0] . "\n";
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_NEXT);     /* 2 */
echo "NEXT: " . $r[0] . "\n";
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_LAST);     /* 3 */
echo "LAST: " . $r[0] . "\n";
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_PRIOR);    /* 2 */
echo "PRIOR: " . $r[0] . "\n";
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_FIRST);    /* 1 */
echo "FIRST: " . $r[0] . "\n";
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_ABS, 3);   /* 3 */
echo "ABS(3): " . $r[0] . "\n";
$r = $stmt->fetch(PDO::FETCH_NUM, PDO::FETCH_ORI_REL, -1);  /* 2 */
echo "REL(-1): " . $r[0] . "\n";

/* Cleanup */
$stmt->closeCursor();
unset($stmt);
unset($stmt2);
unset($stmt3);
unset($pdo);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: EOF detection — NEXT past last row ===
EOF returns false: yes

=== Test 2: BOF detection — PRIOR before first row ===
BOF returns false: yes

=== Test 3: Cursor usable after EOF — reposition to LAST ===
Usable after EOF: yes, id=3

=== Test 4: Cursor usable after BOF — reposition to FIRST ===
Usable after BOF: yes, id=1

=== Test 5: Empty result set ===
Empty NEXT: false (correct)
Empty FIRST: false (correct)
Empty LAST: false (correct)

=== Test 6: Single row — FIRST==LAST==ABS(1) ===
Single FIRST: id=42
Single LAST: id=42
Single ABS(1): id=42

=== Test 7: Out-of-bounds ABS ===
ABS(0): false (correct)
ABS(99): false (correct)
After OOB, FIRST: yes, id=1

=== Test 8: REL beyond boundaries ===
REL(-99) from row 1: false (BOF, correct)
REL(+99) from BOF: false (EOF, correct)
After REL OOB, ABS(2): yes, id=2

=== Test 9: Cross-orientation navigation ===
NEXT: 1
NEXT: 2
LAST: 3
PRIOR: 2
FIRST: 1
ABS(3): 3
REL(-1): 2

Done.

--CLEAN--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP TABLE scroll_edge");
@$pdo->exec("DROP TABLE scroll_empty");
@$pdo->exec("DROP TABLE scroll_one");
unset($pdo);
?>

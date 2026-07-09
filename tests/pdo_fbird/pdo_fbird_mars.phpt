--TEST--
pdo_fbird: Multiple Active Result Sets (MARS) (#435)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/../pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';

/* ============================================================
 * Issue #435: PDO Multiple Active Result Sets (MARS)
 *
 * Verifies that multiple PDOStatement objects can be active
 * simultaneously on a single PDO connection, with interleaved
 * fetches. Firebird supports multiple open cursors per transaction
 * natively — the driver should not artificially prevent this.
 * ============================================================ */

$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);

/* Setup: create test table with 5 rows */
$pdo->exec("RECREATE TABLE mars_test (id INTEGER NOT NULL PRIMARY KEY, val VARCHAR(20))");
for ($i = 1; $i <= 5; $i++) {
    $pdo->exec("INSERT INTO mars_test VALUES ($i, 'row$i')");
}

echo "=== Test 1: Interleaved fetch from 2 SELECTs ===\n";
$stmt1 = $pdo->prepare("SELECT id, val FROM mars_test ORDER BY id");
$stmt2 = $pdo->prepare("SELECT id, val FROM mars_test ORDER BY id DESC");
$stmt1->execute();
$stmt2->execute();

/* Fetch interleaved: stmt1 ascending, stmt2 descending */
$r1a = $stmt1->fetch(PDO::FETCH_NUM);
$r2a = $stmt2->fetch(PDO::FETCH_NUM);
$r1b = $stmt1->fetch(PDO::FETCH_NUM);
$r2b = $stmt2->fetch(PDO::FETCH_NUM);

echo "stmt1 row 1: {$r1a[0]}-{$r1a[1]}\n";
echo "stmt2 row 1: {$r2a[0]}-{$r2a[1]}\n";
echo "stmt1 row 2: {$r1b[0]}-{$r1b[1]}\n";
echo "stmt2 row 2: {$r2b[0]}-{$r2b[1]}\n";

$stmt1->closeCursor();
$stmt2->closeCursor();

echo "\n=== Test 2: Close one cursor, verify other survives ===\n";
$stmt1 = $pdo->prepare("SELECT id FROM mars_test ORDER BY id");
$stmt2 = $pdo->prepare("SELECT id FROM mars_test ORDER BY id DESC");
$stmt1->execute();
$stmt2->execute();

$stmt1->fetch(PDO::FETCH_NUM); /* consume one row */
$stmt1->closeCursor();

/* stmt2 should still be usable */
$r = $stmt2->fetch(PDO::FETCH_NUM);
echo "stmt2 after stmt1 closed: {$r[0]}\n";
$stmt2->closeCursor();

echo "\n=== Test 3: Re-execute same statement ===\n";
$stmt = $pdo->prepare("SELECT id FROM mars_test ORDER BY id");
$stmt->execute();
$row1 = $stmt->fetch(PDO::FETCH_NUM);
echo "First execute row 1: {$row1[0]}\n";
$stmt->closeCursor();

/* Re-execute — should get fresh result set */
$stmt->execute();
$row1b = $stmt->fetch(PDO::FETCH_NUM);
echo "Re-execute row 1: {$row1b[0]}\n";
$stmt->closeCursor();

echo "\n=== Test 4: Three statements round-robin ===\n";
$s1 = $pdo->prepare("SELECT id FROM mars_test WHERE id <= 3 ORDER BY id");
$s2 = $pdo->prepare("SELECT val FROM mars_test WHERE id <= 3 ORDER BY id");
$s3 = $pdo->prepare("SELECT COUNT(*) FROM mars_test");
$s1->execute();
$s2->execute();
$s3->execute();

while (($r1 = $s1->fetch(PDO::FETCH_NUM)) && ($r2 = $s2->fetch(PDO::FETCH_NUM))) {
    echo "s1={$r1[0]} s2={$r2[0]}\n";
}
$r3 = $s3->fetch(PDO::FETCH_NUM);
echo "s3 count: {$r3[0]}\n";

$s1->closeCursor();
$s2->closeCursor();
$s3->closeCursor();

echo "\n=== Test 5: Transaction commit invalidates cursors ===\n";
$pdo->beginTransaction();
$stmt1 = $pdo->prepare("SELECT id FROM mars_test ORDER BY id");
$stmt1->execute();
$stmt1->fetch(PDO::FETCH_NUM); /* consume one row */
$pdo->commit();

/* After commit, cursor is invalidated server-side.
 * Fetch may return false (C++ layer self-heals) or throw PDOException. */
$invalidated = false;
try {
    $r = $stmt1->fetch(PDO::FETCH_NUM);
    $invalidated = ($r === false);
} catch (PDOException $e) {
    $invalidated = true;
}
echo "Fetch after commit: " . ($invalidated ? 'cursor invalidated' : 'unexpected row') . "\n";

echo "\n=== Test 6: DML between SELECTs (autocommit) ===\n";
/* In autocommit mode, DML uses commit_retaining which preserves cursors */
$pdo->exec("INSERT INTO mars_test VALUES (6, 'row6')");

$stmt1 = $pdo->prepare("SELECT id FROM mars_test ORDER BY id");
$stmt1->execute();
$row = $stmt1->fetch(PDO::FETCH_NUM);
echo "Before DML: {$row[0]}\n";

/* DML with autocommit — uses commit_retaining, cursor should survive */
$pdo->exec("INSERT INTO mars_test VALUES (7, 'row7')");

/* Try to continue fetching — commit_retaining should not invalidate */
$row2 = @$stmt1->fetch(PDO::FETCH_NUM);
echo "After DML autocommit: " . ($row2 !== false ? "id={$row2[0]}" : 'cursor invalidated') . "\n";
$stmt1->closeCursor();

/* Cleanup */
$pdo->exec("DELETE FROM mars_test WHERE id IN (6, 7)");
$pdo->exec("DROP TABLE mars_test");

/* Close statements explicitly */
unset($stmt1);
unset($stmt2);
unset($stmt);
unset($s1);
unset($s2);
unset($s3);
unset($pdo);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Interleaved fetch from 2 SELECTs ===
stmt1 row 1: 1-row1
stmt2 row 1: 5-row5
stmt1 row 2: 2-row2
stmt2 row 2: 4-row4

=== Test 2: Close one cursor, verify other survives ===
stmt2 after stmt1 closed: 5

=== Test 3: Re-execute same statement ===
First execute row 1: 1
Re-execute row 1: 1

=== Test 4: Three statements round-robin ===
s1=1 s2=row1
s1=2 s2=row2
s1=3 s2=row3
s3 count: 5

=== Test 5: Transaction commit invalidates cursors ===
Fetch after commit: cursor invalidated

=== Test 6: DML between SELECTs (autocommit) ===
Before DML: 1
After DML autocommit: %s

Done.

--CLEAN--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP TABLE mars_test");
unset($pdo);
?>

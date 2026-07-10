--TEST--
pdo_fbird: FB4+ DECFLOAT(16/34) precision and SET BIND coverage (#417)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/../fb_version_probe.inc';
if (!fb_server_supports('DECFLOAT')) die('skip DECFLOAT not supported');
?>
--FILE--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';

/* ============================================================
 * Issue #417: FB4 DECFLOAT(16/34) native type support
 *
 * These tests verify precision is preserved in the string representation
 * and that SET BIND coercion works correctly.
 *
 * DECFLOAT(16): IEEE 754 decimal64 - 16 significant digits
 * DECFLOAT(34): IEEE 754 decimal128 - 34 significant digits
 *
 * Note: PDO driver returns DECFLOAT as strings (not Firebird\DecFloat objects).
 * The procedural API returns Firebird\DecFloat objects on PHP 8.3+.
 * ============================================================ */

$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

$pdo->exec("RECREATE TABLE decfloat_test (
    id INTEGER NOT NULL PRIMARY KEY,
    df16 DECFLOAT(16),
    df34 DECFLOAT(34)
)");

echo "=== Test 1: Basic precision — DECFLOAT(16) ===\n";
$pdo->exec("INSERT INTO decfloat_test VALUES (1, 3.141592653589793, 3.141592653589793238462643383279502)");
$stmt = $pdo->query("SELECT df16, df34 FROM decfloat_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "df16: " . $row[0] . "\n";
echo "df34: " . $row[1] . "\n";
echo "df16 type: " . gettype($row[0]) . "\n";
echo "df34 type: " . gettype($row[1]) . "\n";

echo "\n=== Test 2: Zero and negative ===\n";
$pdo->exec("INSERT INTO decfloat_test VALUES (2, 0, 0)");
$pdo->exec("INSERT INTO decfloat_test VALUES (3, -3.14, -3.14)");
$stmt = $pdo->query("SELECT df16, df34 FROM decfloat_test WHERE id IN (2,3) ORDER BY id");
while ($row = $stmt->fetch(PDO::FETCH_NUM)) {
    echo "df16=$row[0], df34=$row[1]\n";
}

echo "\n=== Test 3: Large DECFLOAT(34) — 34 significant digits ===\n";
$big34 = '1234567890123456789012345678901234'; /* 34 digits */
$pdo->exec("INSERT INTO decfloat_test VALUES (4, 0, {$big34})");
$stmt = $pdo->query("SELECT df34 FROM decfloat_test WHERE id = 4");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "df34 (34 digits): " . $row[0] . "\n";
echo "precision preserved: " . ($row[0] === $big34 ? 'yes' : 'no') . "\n";

echo "\n=== Test 4: DECFLOAT(16) max significant digits (16) ===\n";
$big16 = '1234567890123456'; /* 16 digits */
$pdo->exec("INSERT INTO decfloat_test VALUES (5, {$big16}, 0)");
$stmt = $pdo->query("SELECT df16 FROM decfloat_test WHERE id = 5");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "df16 (16 digits): " . $row[0] . "\n";

echo "\n=== Test 5: Scientific notation values ===\n";
$pdo->exec("INSERT INTO decfloat_test VALUES (6, 1.23E10, 1.23456789012345678901234567890123E-20)");
$stmt = $pdo->query("SELECT df16, df34 FROM decfloat_test WHERE id = 6");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "df16 (1.23E10): " . $row[0] . "\n";
echo "df34 (1.23E-20): " . $row[1] . "\n";

echo "\n=== Test 6: NULL values ===\n";
$pdo->exec("INSERT INTO decfloat_test (id, df16, df34) VALUES (7, NULL, NULL)");
$stmt = $pdo->query("SELECT df16, df34 FROM decfloat_test WHERE id = 7");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "df16 is NULL: " . ($row[0] === null ? 'yes' : 'no') . "\n";
echo "df34 is NULL: " . ($row[1] === null ? 'yes' : 'no') . "\n";

echo "\n=== Test 7: SET BIND OF DECFLOAT TO DOUBLE PRECISION ===\n";
/* SET BIND coerces DECFLOAT to DOUBLE PRECISION (lossy for >15 digits).
 * Must use a separate connection — SET BIND is attachment-scoped. */
$pdo2 = pdo_fbird_connect();
$pdo2->exec("SET BIND OF DECFLOAT TO DOUBLE PRECISION");
$stmt = $pdo2->query("SELECT df16, df34 FROM decfloat_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "After SET BIND TO DOUBLE - df16: " . $row[0] . "\n";
echo "After SET BIND TO DOUBLE - df34: " . $row[1] . "\n";

echo "\n=== Test 8: SET BIND OF DECFLOAT TO VARCHAR ===\n";
$pdo3 = pdo_fbird_connect();
$pdo3->exec("SET BIND OF DECFLOAT TO VARCHAR");
$stmt = $pdo3->query("SELECT df16, df34 FROM decfloat_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "After SET BIND TO VARCHAR - df16: " . $row[0] . "\n";
echo "After SET BIND TO VARCHAR - df34: " . $row[1] . "\n";

/* Close all statements and connections explicitly — PDOStatement holds a
 * ref to its parent PDO, so unset $stmt before $pdo to actually release
 * the connection before subsequent tests (e.g. service_backup_restore). */
unset($stmt);
unset($pdo3);
unset($pdo2);
unset($pdo);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Basic precision — DECFLOAT(16) ===
df16: %s
df34: %s
df16 type: string
df34 type: string

=== Test 2: Zero and negative ===
df16=%s, df34=%s
df16=%s, df34=%s

=== Test 3: Large DECFLOAT(34) — 34 significant digits ===
df34 (34 digits): %s
precision preserved: %s

=== Test 4: DECFLOAT(16) max significant digits (16) ===
df16 (16 digits): %s

=== Test 5: Scientific notation values ===
df16 (1.23E10): %s
df34 (1.23E-20): %s

=== Test 6: NULL values ===
df16 is NULL: yes
df34 is NULL: yes

=== Test 7: SET BIND OF DECFLOAT TO DOUBLE PRECISION ===
After SET BIND TO DOUBLE - df16: %s
After SET BIND TO DOUBLE - df34: %s

=== Test 8: SET BIND OF DECFLOAT TO VARCHAR ===
After SET BIND TO VARCHAR - df16: %s
After SET BIND TO VARCHAR - df34: %s

Done.

--CLEAN--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP TABLE decfloat_test");
unset($pdo);
?>

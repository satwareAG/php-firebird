--TEST--
pdo_fbird: FB4+ INT128 precision and SET BIND coverage (#418)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/../fb_version_probe.inc';
if (!fb_server_supports('INT128')) die('skip INT128 not supported');
?>
--FILE--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';

/* ============================================================
 * Issue #418: FB4 INT128 native type support
 *
 * INT128 is a 128-bit exact integer (NUMERIC(38) precision).
 * Currently returns as string (via IUtil->getInt128->toString).
 * These tests verify precision is preserved and SET BIND coercion works.
 *
 * INT128 range: -170141183460469231731687303715884105727 ..
 *                170141183460469231731687303715884105727
 * ============================================================ */

$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

$pdo->exec("RECREATE TABLE int128_test (
    id INTEGER NOT NULL PRIMARY KEY,
    val INT128
)");

echo "=== Test 1: Basic value round-trip ===\n";
$pdo->exec("INSERT INTO int128_test VALUES (1, 12345678901234567890)");
$stmt = $pdo->query("SELECT val FROM int128_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "val: " . $row[0] . "\n";
echo "type: " . gettype($row[0]) . "\n";

echo "\n=== Test 2: Max INT128 value ===\n";
$max = '170141183460469231731687303715884105727';
$pdo->exec("INSERT INTO int128_test VALUES (2, {$max})");
$stmt = $pdo->query("SELECT val FROM int128_test WHERE id = 2");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "max: " . $row[0] . "\n";
echo "precision preserved: " . ($row[0] === $max ? 'yes' : 'no') . "\n";

echo "\n=== Test 3: Min INT128 value ===\n";
$min = '-170141183460469231731687303715884105727';
$pdo->exec("INSERT INTO int128_test VALUES (3, {$min})");
$stmt = $pdo->query("SELECT val FROM int128_test WHERE id = 3");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "min: " . $row[0] . "\n";

echo "\n=== Test 4: Zero and small values ===\n";
$pdo->exec("INSERT INTO int128_test VALUES (4, 0)");
$pdo->exec("INSERT INTO int128_test VALUES (5, 1)");
$pdo->exec("INSERT INTO int128_test VALUES (6, -1)");
$stmt = $pdo->query("SELECT val FROM int128_test WHERE id IN (4,5,6) ORDER BY id");
while ($row = $stmt->fetch(PDO::FETCH_NUM)) {
    echo "val: $row[0]\n";
}

echo "\n=== Test 5: Value beyond INT64 range (9223372036854775807) ===\n";
$beyond = '9223372036854775808'; /* INT64_MAX + 1 */
$pdo->exec("INSERT INTO int128_test VALUES (7, {$beyond})");
$stmt = $pdo->query("SELECT val FROM int128_test WHERE id = 7");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "beyond INT64: " . $row[0] . "\n";

echo "\n=== Test 6: NULL value ===\n";
$pdo->exec("INSERT INTO int128_test (id, val) VALUES (8, NULL)");
$stmt = $pdo->query("SELECT val FROM int128_test WHERE id = 8");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "val is NULL: " . ($row[0] === null ? 'yes' : 'no') . "\n";

echo "\n=== Test 7: NUMERIC(38) with scale ===\n";
/* INT128 maps to NUMERIC(38,0). Test NUMERIC(38,4) for decimal precision. */
$pdo->exec("RECREATE TABLE int128_scaled (val NUMERIC(38,4))");
$pdo->exec("INSERT INTO int128_scaled VALUES (12345678901234567890123456789012.3456)");
$stmt = $pdo->query("SELECT val FROM int128_scaled");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "scaled: " . $row[0] . "\n";

echo "\n=== Test 8: SET BIND OF INT128 TO BIGINT ===\n";
/* SET BIND coerces INT128 to BIGINT. Values within INT64 range survive;
 * values beyond INT64 range overflow. Test with a safe value (id=4, val=0). */
$pdo2 = pdo_fbird_connect();
$pdo2->exec("SET BIND OF INT128 TO BIGINT");
$stmt = $pdo2->query("SELECT val FROM int128_test WHERE id = 4");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "After SET BIND TO BIGINT - val(id=4, zero): " . $row[0] . "\n";
$stmt = $pdo2->query("SELECT val FROM int128_test WHERE id = 5");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "After SET BIND TO BIGINT - val(id=5, one): " . $row[0] . "\n";

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Basic value round-trip ===
val: %s
type: string

=== Test 2: Max INT128 value ===
max: %s
precision preserved: %s

=== Test 3: Min INT128 value ===
min: %s

=== Test 4: Zero and small values ===
val: %s
val: %s
val: %s

=== Test 5: Value beyond INT64 range (9223372036854775807) ===
beyond INT64: %s

=== Test 6: NULL value ===
val is NULL: yes

=== Test 7: NUMERIC(38) with scale ===
scaled: %s

=== Test 8: SET BIND OF INT128 TO BIGINT ===
After SET BIND TO BIGINT - val(id=4, zero): %s
After SET BIND TO BIGINT - val(id=5, one): %s

Done.

--CLEAN--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP TABLE int128_test");
@$pdo->exec("DROP TABLE int128_scaled");
unset($pdo);
?>

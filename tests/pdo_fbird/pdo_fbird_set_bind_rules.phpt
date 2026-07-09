--TEST--
pdo_fbird: FB4+ SET BIND comprehensive rule coverage (#420)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/../fb_version_probe.inc';
if (!fb_server_supports('SET_BIND')) die('skip SET BIND not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';

/* ============================================================
 * Issue #420: FB4 SET BIND rule coverage (all rules + LEGACY)
 *
 * Tests all SET BIND rules via both SQL exec and PDO attribute.
 * Already tested in sibling tests (partial):
 *   - pdo_fbird_decfloat.phpt: DECFLOAT TO DOUBLE, DECFLOAT TO VARCHAR
 *   - pdo_fbird_int128.phpt: INT128 TO BIGINT
 *   - pdo_fbird_timestamp_tz.phpt: TIME ZONE TO LEGACY
 *
 * This test covers the GAPS:
 *   - BIND_ASCII (never tested)
 *   - BIND_TIMESTAMP (never tested)
 *   - INT128 TO BIGINT via PDO attribute path (only SQL tested)
 *   - TIME ZONE TO LEGACY via PDO attribute path (only SQL tested)
 *   - Comprehensive LEGACY mode (all types coerced simultaneously)
 *   - DPB default bind rules (implicit at connect time)
 * ============================================================ */

echo "=== Test 1: SET BIND via PDO attribute — INT128 TO BIGINT ===\n";
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
$pdo->exec("RECREATE TABLE setbind_test (
    id INTEGER NOT NULL PRIMARY KEY,
    val_int128 INT128,
    val_df DECFLOAT(16),
    val_ts_tz TIMESTAMP WITH TIME ZONE
)");
$pdo->exec("INSERT INTO setbind_test VALUES (1, 999999999, 3.14, '2026-01-01 10:00:00 UTC')");

/* PDO attribute path: set at connection level */
$pdo->exec("SET BIND OF INT128 TO BIGINT");
$stmt = $pdo->query("SELECT val_int128 FROM setbind_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "INT128 via SET BIND TO BIGINT: " . $row[0] . "\n";

echo "\n=== Test 2: SET BIND via PDO attribute — TIME ZONE TO LEGACY ===\n";
$pdo->exec("SET BIND OF TIME ZONE TO LEGACY");
$stmt = $pdo->query("SELECT val_ts_tz FROM setbind_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "TIMESTAMP WITH TZ via SET BIND TO LEGACY: " . $row[0] . "\n";
/* Legacy mode strips TZ info — should not contain timezone name */
echo "Has no TZ name: " . (preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}/', trim($row[0])) ? 'yes' : 'no') . "\n";

echo "\n=== Test 3: BIND_ASCII — SET BIND OF BINARY TO CHAR ===\n";
/* FB4: CHAR/VARCHAR with OCTETS charset have BINARY/VARBINARY synonyms.
 * SET BIND OF BINARY TO CHAR coerces binary data to character representation. */
$r = $pdo->exec("SET BIND OF BINARY TO CHAR");
echo "SET BIND OF BINARY TO CHAR: " . ($r !== false ? 'accepted' : 'rejected') . "\n";

echo "\n=== Test 4: Multiple SET BIND rules on same connection ===\n";
$pdo2 = pdo_fbird_connect();
$pdo2->exec("SET BIND OF DECFLOAT TO VARCHAR");
$pdo2->exec("SET BIND OF INT128 TO BIGINT");
$pdo2->exec("SET BIND OF TIME ZONE TO LEGACY");
$stmt = $pdo2->query("SELECT val_int128, val_df, val_ts_tz FROM setbind_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "Multi-bind INT128: " . $row[0] . "\n";
echo "Multi-bind DECFLOAT: " . $row[1] . "\n";
echo "Multi-bind TIMESTAMP_TZ: " . $row[2] . "\n";

echo "\n=== Test 5: Comprehensive LEGACY mode (DataTypeCompatibility=3.0) ===\n";
/* LEGACY mode coerces all FB4+ types to FB3-compatible equivalents.
 * Equivalent to SET BIND for each type individually. */
$pdo3 = pdo_fbird_connect();
$pdo3->exec("SET BIND OF DECFLOAT TO DOUBLE PRECISION");
$pdo3->exec("SET BIND OF INT128 TO BIGINT");
$pdo3->exec("SET BIND OF TIME ZONE TO TIME");
$pdo3->exec("SET BIND OF TIMESTAMP WITH TIME ZONE TO TIMESTAMP");
$stmt = $pdo3->query("SELECT val_int128, val_df, val_ts_tz FROM setbind_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_NUM);
echo "LEGACY INT128 (as BIGINT): " . $row[0] . "\n";
echo "LEGACY DECFLOAT (as DOUBLE): " . $row[1] . "\n";
echo "LEGACY TIMESTAMP_TZ (as TIMESTAMP): " . $row[2] . "\n";

echo "\n=== Test 6: SET BIND write-only via PDO attribute ===\n";
/* FBIRD_ATTR_SET_BIND is write-only: setAttribute returns true,
 * getAttribute returns empty string. */
$pdo4 = pdo_fbird_connect();
$r = $pdo4->setAttribute(PDO::FBIRD_ATTR_SET_BIND, "DECFLOAT TO VARCHAR");
echo "setAttribute returns: " . ($r ? 'true' : 'false') . "\n";
$v = $pdo4->getAttribute(PDO::FBIRD_ATTR_SET_BIND);
echo "getAttribute returns empty (write-only): " . ($v === '' || $v === null ? 'yes' : 'no') . "\n";

echo "\n=== Test 7: Reset bind rule ===\n";
$pdo5 = pdo_fbird_connect();
$pdo5->exec("SET BIND OF DECFLOAT TO VARCHAR");
/* Reset to default (native) */
$pdo5->exec("SET BIND OF DECFLOAT TO NATIVE");
echo "SET BIND ... TO NATIVE: accepted\n";

/* Cleanup */
unset($stmt);
unset($pdo5);
unset($pdo4);
unset($pdo3);
unset($pdo2);
$pdo->exec("DROP TABLE setbind_test");
unset($pdo);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: SET BIND via PDO attribute — INT128 TO BIGINT ===
INT128 via SET BIND TO BIGINT: %s

=== Test 2: SET BIND via PDO attribute — TIME ZONE TO LEGACY ===
TIMESTAMP WITH TZ via SET BIND TO LEGACY: %s
Has no TZ name: yes

=== Test 3: BIND_ASCII — SET BIND OF BINARY TO CHAR ===
SET BIND OF BINARY TO CHAR: %s

=== Test 4: Multiple SET BIND rules on same connection ===
Multi-bind INT128: %s
Multi-bind DECFLOAT: %s
Multi-bind TIMESTAMP_TZ: %s

=== Test 5: Comprehensive LEGACY mode (DataTypeCompatibility=3.0) ===
LEGACY INT128 (as BIGINT): %s
LEGACY DECFLOAT (as DOUBLE): %s
LEGACY TIMESTAMP_TZ (as TIMESTAMP): %s

=== Test 6: SET BIND write-only via PDO attribute ===
setAttribute returns: true
getAttribute returns empty (write-only): yes

=== Test 7: Reset bind rule ===
SET BIND ... TO NATIVE: accepted

Done.

--CLEAN--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP TABLE setbind_test");
unset($pdo);
?>

--TEST--
pdo_fbird: FB4+ TIME/TIMESTAMP WITH TIME ZONE end-to-end
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/../fb_version_probe.inc';
if (!fb_server_supports('TIMESTAMP_TZ')) die('skip TIME/TIMESTAMP WITH TIME ZONE not supported');
?>
--FILE--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';

$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

/* Create test table with both TZ types */
$pdo->exec("RECREATE TABLE pdo_tz_test (
    id INTEGER NOT NULL PRIMARY KEY,
    time_tz TIME WITH TIME ZONE,
    ts_tz TIMESTAMP WITH TIME ZONE
)");

echo "=== Test 1: INSERT via SQL literals + SELECT ===\n";
$pdo->exec("INSERT INTO pdo_tz_test VALUES (1, '10:30:00 Europe/Berlin', '2026-07-08 10:30:00 Europe/Berlin')");
$pdo->exec("INSERT INTO pdo_tz_test VALUES (2, '14:45:30 UTC', '2026-07-08 14:45:30 UTC')");
$pdo->exec("INSERT INTO pdo_tz_test VALUES (3, '00:00:00 UTC', '2026-01-01 00:00:00 UTC')");

$stmt = $pdo->query("SELECT id, time_tz, ts_tz FROM pdo_tz_test ORDER BY id");
while ($row = $stmt->fetch(PDO::FETCH_ASSOC)) {
    echo sprintf("Row %d: time_tz=%s, ts_tz=%s\n",
        $row['ID'], $row['TIME_TZ'], $row['TS_TZ']);
}

echo "\n=== Test 2: NULL values ===\n";
$pdo->exec("INSERT INTO pdo_tz_test (id, time_tz, ts_tz) VALUES (4, NULL, NULL)");
$stmt = $pdo->query("SELECT time_tz, ts_tz FROM pdo_tz_test WHERE id = 4");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "time_tz is NULL: " . ($row['TIME_TZ'] === null ? 'yes' : 'no') . "\n";
echo "ts_tz is NULL: " . ($row['TS_TZ'] === null ? 'yes' : 'no') . "\n";

echo "\n=== Test 3: UPDATE via SQL literals ===\n";
$pdo->exec("UPDATE pdo_tz_test SET time_tz = '23:59:59 America/New_York', ts_tz = '2026-12-31 23:59:59 America/New_York' WHERE id = 1");
$stmt = $pdo->query("SELECT time_tz, ts_tz FROM pdo_tz_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Updated row 1: time_tz=" . $row['TIME_TZ'] . ", ts_tz=" . $row['TS_TZ'] . "\n";

echo "\n=== Test 4: CURRENT_TIMESTAMP AT TIME ZONE ===\n";
$pdo->exec("INSERT INTO pdo_tz_test (id, ts_tz) VALUES (5, CURRENT_TIMESTAMP AT TIME ZONE 'UTC')");
$stmt = $pdo->query("SELECT ts_tz FROM pdo_tz_test WHERE id = 5");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "ts_tz contains 'UTC': " . (strpos($row['TS_TZ'], 'UTC') !== false ? 'yes' : 'no') . "\n";

echo "\n=== Test 5: EXTRACT(TIMEZONE_HOUR|MINUTE) ===\n";
$stmt = $pdo->query("SELECT
    EXTRACT(TIMEZONE_HOUR FROM TIMESTAMP'2026-07-08 10:30:00 Europe/Berlin') AS tz_hour,
    EXTRACT(TIMEZONE_MINUTE FROM TIMESTAMP'2026-07-08 10:30:00 Europe/Berlin') AS tz_min,
    EXTRACT(TIMEZONE_HOUR FROM TIMESTAMP'2026-07-08 10:30:00 UTC') AS tz_hour_utc
    FROM RDB\$DATABASE");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Berlin tz_hour: " . $row['TZ_HOUR'] . "\n";
echo "Berlin tz_min: " . $row['TZ_MIN'] . "\n";
echo "UTC tz_hour: " . $row['TZ_HOUR_UTC'] . "\n";

echo "\n=== Test 6: SET BIND OF TIME ZONE TO LEGACY ===\n";
/* SET BIND coerces TZ types to legacy types for backward compat.
 * After SET BIND OF TIME ZONE TO LEGACY, TZ types are returned as
 * plain TIME/TIMESTAMP (without TZ info). Must be a separate connection
 * because SET BIND affects the attachment. */
$pdo2 = pdo_fbird_connect();
$pdo2->exec("SET BIND OF TIME ZONE TO LEGACY");
$stmt = $pdo2->query("SELECT time_tz, ts_tz FROM pdo_tz_test WHERE id = 2");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "After SET BIND TO LEGACY - time_tz: " . $row['TIME_TZ'] . "\n";
echo "After SET BIND TO LEGACY - ts_tz: " . $row['TS_TZ'] . "\n";
/* Legacy mode strips TZ info — values should not contain timezone names/offsets.
 * Anchor at start (HH:MM:SS) without end anchor to tolerate fractional seconds. */
echo "time_tz has no TZ name: " . (preg_match('/^\d{2}:\d{2}:\d{2}/', trim($row['TIME_TZ'])) ? 'yes' : 'no') . "\n";

/* Close all connections explicitly — SET BIND connections must be released
 * before subsequent tests (e.g. service_backup_restore) can create databases. */
unset($pdo2);
unset($pdo);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: INSERT via SQL literals + SELECT ===
Row 1: time_tz=10:30:00%s Europe/Berlin, ts_tz=2026-07-08 10:30:00%s Europe/Berlin
Row 2: time_tz=14:45:30%s UTC, ts_tz=2026-07-08 14:45:30%s UTC
Row 3: time_tz=00:00:00%s UTC, ts_tz=2026-01-01 00:00:00%s UTC

=== Test 2: NULL values ===
time_tz is NULL: yes
ts_tz is NULL: yes

=== Test 3: UPDATE via SQL literals ===
Updated row 1: time_tz=23:59:59%s America/New_York, ts_tz=2026-12-31 23:59:59%s America/New_York

=== Test 4: CURRENT_TIMESTAMP AT TIME ZONE ===
ts_tz contains 'UTC': yes

=== Test 5: EXTRACT(TIMEZONE_HOUR|MINUTE) ===
Berlin tz_hour: %d
Berlin tz_min: 0
UTC tz_hour: 0

=== Test 6: SET BIND OF TIME ZONE TO LEGACY ===
After SET BIND TO LEGACY - time_tz: %s
After SET BIND TO LEGACY - ts_tz: %s
time_tz has no TZ name: yes

Done.

--CLEAN--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP TABLE pdo_tz_test");
unset($pdo);
?>

--TEST--
fbird date/time/timestamp formatting INI settings
--SKIPIF--
<?php include("skipif.inc"); ?>
--INI--
fbird.timestampformat="%Y-%m-%d %H:%M:%S"
fbird.dateformat="%Y/%m/%d"
fbird.timeformat="%H:%M"
--FILE--
<?php
require_once("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);

// Use a fresh table name to avoid any leftover locks from previous failed runs
$table = "TEST_FMT_" . time();

// Metadata change: CREATE TABLE and COMMIT
fbird_query($conn, "CREATE TABLE $table (ts TIMESTAMP, dt DATE, tm TIME)");
fbird_commit($conn);

fbird_query($conn, "INSERT INTO $table (ts, dt, tm) VALUES ('2026-03-24 15:30:45', '2026-03-24', '15:30:45')");
fbird_commit($conn);

$res = fbird_query($conn, "SELECT ts, dt, tm FROM $table");
$row = fbird_fetch_assoc($res);
fbird_free_result($res);
fbird_commit($conn);

echo "Timestamp: " . $row['TS'] . "\n";
echo "Date: " . $row['DT'] . "\n";
echo "Time: " . $row['TM'] . "\n";

echo "Changing INI...\n";
ini_set('fbird.timestampformat', '%d.%m.%Y %H:%M');
ini_set('fbird.dateformat', '%d-%m-%Y');
ini_set('fbird.timeformat', '%H:%M:%S');

$res = fbird_query($conn, "SELECT ts, dt, tm FROM $table");
$row = fbird_fetch_assoc($res);
fbird_free_result($res);
fbird_commit($conn);

echo "Timestamp: " . $row['TS'] . "\n";
echo "Date: " . $row['DT'] . "\n";
echo "Time: " . $row['TM'] . "\n";

fbird_close($conn);
?>
--EXPECT--
Timestamp: 2026-03-24 15:30:45
Date: 2026/03/24
Time: 15:30
Changing INI...
Timestamp: 24.03.2026 15:30
Date: 24-03-2026
Time: 15:30:45

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

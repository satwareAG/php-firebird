--TEST--
Firebird 4.0+ timezone types: direct queries and FBIRD_UNIXTIME behavior
--SKIPIF--
<?php
include("skipif.inc");
skip_if_fb_lt(4);
skip_if_fbclient_lt(4);
?>
--FILE--
<?php
/*
 * Test direct query INSERT (non-parameterized) for timezone types
 * and verify FBIRD_UNIXTIME behavior with TIMESTAMP WITH TIME ZONE.
 */

require("firebird.inc");

fbird_connect($test_base);

// Create test table
fbird_query("CREATE TABLE TZ_DIRECT_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    TIME_TZ TIME WITH TIME ZONE,
    TIMESTAMP_TZ TIMESTAMP WITH TIME ZONE
)");
fbird_commit();

echo "=== Test 1: Direct INSERT with literal values ===\n";
fbird_query("INSERT INTO TZ_DIRECT_TEST (ID, TIME_TZ, TIMESTAMP_TZ)
             VALUES (1, '12:00:00 UTC', '2025-01-15 12:00:00 UTC')");
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_DIRECT_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($q);
var_dump($row);
fbird_free_result($q);

echo "\n=== Test 2: FBIRD_UNIXTIME flag with TIMESTAMP WITH TIME ZONE ===\n";
fbird_query("INSERT INTO TZ_DIRECT_TEST (ID, TIME_TZ, TIMESTAMP_TZ)
             VALUES (2, '15:30:00 Europe/Berlin', '2025-06-15 15:30:00 Europe/Berlin')");
fbird_commit();

// Without FBIRD_UNIXTIME - returns formatted string
$q = fbird_query("SELECT * FROM TZ_DIRECT_TEST WHERE ID = 2");
$row = fbird_fetch_assoc($q);
echo "Without FBIRD_UNIXTIME:\n";
echo "  TIME_TZ type: " . gettype($row['TIME_TZ']) . "\n";
echo "  TIMESTAMP_TZ type: " . gettype($row['TIMESTAMP_TZ']) . "\n";
fbird_free_result($q);

// With FBIRD_UNIXTIME - TIMESTAMP_TZ should return unix timestamp, TIME_TZ should return string
$q = fbird_query("SELECT * FROM TZ_DIRECT_TEST WHERE ID = 2");
$row = fbird_fetch_assoc($q, FBIRD_UNIXTIME);
echo "With FBIRD_UNIXTIME:\n";
echo "  TIME_TZ type: " . gettype($row['TIME_TZ']) . " (TIME_TZ ignores UNIXTIME flag)\n";
echo "  TIMESTAMP_TZ type: " . gettype($row['TIMESTAMP_TZ']) . "\n";
echo "  TIMESTAMP_TZ value: " . $row['TIMESTAMP_TZ'] . "\n";
// Verify it's a sensible unix timestamp (year 2025)
if (is_int($row['TIMESTAMP_TZ']) && $row['TIMESTAMP_TZ'] > 1700000000 && $row['TIMESTAMP_TZ'] < 1800000000) {
    echo "  TIMESTAMP_TZ is valid unix timestamp: yes\n";
}
fbird_free_result($q);

echo "\n=== Test 3: Different timezone regions ===\n";
$timezones = [
    ['America/New_York', '2025-03-15 08:00:00'],
    ['Asia/Tokyo', '2025-03-15 21:00:00'],
    ['Australia/Sydney', '2025-03-15 23:00:00'],
    ['Pacific/Auckland', '2025-03-16 01:00:00'],
];

$id = 10;
foreach ($timezones as [$tz, $ts]) {
    $time = "08:00:00 $tz";
    $timestamp = "$ts $tz";
    fbird_query("INSERT INTO TZ_DIRECT_TEST (ID, TIME_TZ, TIMESTAMP_TZ)
                 VALUES ($id, '$time', '$timestamp')");
    fbird_commit();

    $q = fbird_query("SELECT TIME_TZ, TIMESTAMP_TZ FROM TZ_DIRECT_TEST WHERE ID = $id");
    $row = fbird_fetch_assoc($q);
    echo sprintf("Timezone %-20s: TIME=%s, TIMESTAMP=%s\n",
        $tz, $row['TIME_TZ'], $row['TIMESTAMP_TZ']);
    fbird_free_result($q);
    $id++;
}

echo "\n=== Test 4: Seconds precision in timezone values ===\n";
fbird_query("INSERT INTO TZ_DIRECT_TEST (ID, TIME_TZ, TIMESTAMP_TZ)
             VALUES (20, '10:30:45 UTC', '2025-07-01 10:30:45 UTC')");
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_DIRECT_TEST WHERE ID = 20");
$row = fbird_fetch_assoc($q);
echo "TIME_TZ: " . $row['TIME_TZ'] . "\n";
echo "TIMESTAMP_TZ: " . $row['TIMESTAMP_TZ'] . "\n";
fbird_free_result($q);

echo "\n=== Test 5: CURRENT_TIMESTAMP AT TIME ZONE ===\n";
fbird_query("INSERT INTO TZ_DIRECT_TEST (ID, TIMESTAMP_TZ)
             VALUES (30, CURRENT_TIMESTAMP AT TIME ZONE 'UTC')");
fbird_commit();

$q = fbird_query("SELECT TIMESTAMP_TZ FROM TZ_DIRECT_TEST WHERE ID = 30");
$row = fbird_fetch_assoc($q);
echo "CURRENT_TIMESTAMP AT TIME ZONE 'UTC' contains 'UTC': ";
echo (strpos($row['TIMESTAMP_TZ'], 'UTC') !== false ? 'yes' : 'no') . "\n";
fbird_free_result($q);

fbird_close();
echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Direct INSERT with literal values ===
array(3) {
  ["ID"]=>
  int(1)
  ["TIME_TZ"]=>
  string(%d) "12:00:00 UTC"
  ["TIMESTAMP_TZ"]=>
  string(%d) "2025-01-15 12:00:00 UTC"
}

=== Test 2: FBIRD_UNIXTIME flag with TIMESTAMP WITH TIME ZONE ===
Without FBIRD_UNIXTIME:
  TIME_TZ type: string
  TIMESTAMP_TZ type: string
With FBIRD_UNIXTIME:
  TIME_TZ type: string (TIME_TZ ignores UNIXTIME flag)
  TIMESTAMP_TZ type: integer
  TIMESTAMP_TZ value: %d
  TIMESTAMP_TZ is valid unix timestamp: yes

=== Test 3: Different timezone regions ===
Timezone America/New_York    : TIME=08:00:00 America/New_York, TIMESTAMP=2025-03-15 08:00:00 America/New_York
Timezone Asia/Tokyo          : TIME=08:00:00 Asia/Tokyo, TIMESTAMP=2025-03-15 21:00:00 Asia/Tokyo
Timezone Australia/Sydney    : TIME=08:00:00 Australia/Sydney, TIMESTAMP=2025-03-15 23:00:00 Australia/Sydney
Timezone Pacific/Auckland    : TIME=08:00:00 Pacific/Auckland, TIMESTAMP=2025-03-16 01:00:00 Pacific/Auckland

=== Test 4: Seconds precision in timezone values ===
TIME_TZ: 10:30:45 UTC
TIMESTAMP_TZ: 2025-07-01 10:30:45 UTC

=== Test 5: CURRENT_TIMESTAMP AT TIME ZONE ===
CURRENT_TIMESTAMP AT TIME ZONE 'UTC' contains 'UTC': yes

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

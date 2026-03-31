--TEST--
Firebird 4.0+ TIMESTAMP WITH TIME ZONE and TIME WITH TIME ZONE parameterized binding
--SKIPIF--
<?php
include("skipif.inc");
skip_if_fb_lt(4);
skip_if_fbclient_lt(4);
?>
--FILE--
<?php
/*
 * Test parameterized INSERT and SELECT for timezone types.
 * This verifies the binding implementation in fbird_query_bind.c.
 */

require("firebird.inc");

fbird_connect($test_base);

// Create test table
fbird_query("CREATE TABLE TZ_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    TIME_TZ TIME WITH TIME ZONE,
    TIMESTAMP_TZ TIMESTAMP WITH TIME ZONE
)");
fbird_commit();

echo "=== Test 1: Insert with named timezone (Europe/Berlin) ===\n";
$stmt = fbird_prepare("INSERT INTO TZ_TEST (ID, TIME_TZ, TIMESTAMP_TZ) VALUES (?, ?, ?)");
fbird_execute($stmt, 1, '10:30:00 Europe/Berlin', '2025-06-15 10:30:00 Europe/Berlin');
fbird_free_query($stmt);
fbird_commit();

// Verify insert
$q = fbird_query("SELECT * FROM TZ_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($q);
echo "ID: " . $row['ID'] . "\n";
echo "TIME_TZ: " . $row['TIME_TZ'] . "\n";
echo "TIMESTAMP_TZ: " . $row['TIMESTAMP_TZ'] . "\n";
fbird_free_result($q);

echo "\n=== Test 2: Insert with UTC offset timezone ===\n";
$stmt = fbird_prepare("INSERT INTO TZ_TEST (ID, TIME_TZ, TIMESTAMP_TZ) VALUES (?, ?, ?)");
fbird_execute($stmt, 2, '14:45:30 +02:00', '2025-12-25 14:45:30 +02:00');
fbird_free_query($stmt);
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_TEST WHERE ID = 2");
$row = fbird_fetch_assoc($q);
echo "ID: " . $row['ID'] . "\n";
echo "TIME_TZ: " . $row['TIME_TZ'] . "\n";
echo "TIMESTAMP_TZ: " . $row['TIMESTAMP_TZ'] . "\n";
fbird_free_result($q);

echo "\n=== Test 3: Insert with UTC timezone ===\n";
$stmt = fbird_prepare("INSERT INTO TZ_TEST (ID, TIME_TZ, TIMESTAMP_TZ) VALUES (?, ?, ?)");
fbird_execute($stmt, 3, '00:00:00 UTC', '2025-01-01 00:00:00 UTC');
fbird_free_query($stmt);
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_TEST WHERE ID = 3");
$row = fbird_fetch_assoc($q);
echo "ID: " . $row['ID'] . "\n";
echo "TIME_TZ: " . $row['TIME_TZ'] . "\n";
echo "TIMESTAMP_TZ: " . $row['TIMESTAMP_TZ'] . "\n";
fbird_free_result($q);

echo "\n=== Test 4: Insert with negative UTC offset ===\n";
$stmt = fbird_prepare("INSERT INTO TZ_TEST (ID, TIME_TZ, TIMESTAMP_TZ) VALUES (?, ?, ?)");
fbird_execute($stmt, 4, '08:00:00 -05:00', '2025-07-04 08:00:00 -05:00');
fbird_free_query($stmt);
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_TEST WHERE ID = 4");
$row = fbird_fetch_assoc($q);
echo "ID: " . $row['ID'] . "\n";
echo "TIME_TZ: " . $row['TIME_TZ'] . "\n";
echo "TIMESTAMP_TZ: " . $row['TIMESTAMP_TZ'] . "\n";
fbird_free_result($q);

echo "\n=== Test 5: Verify all rows ===\n";
$q = fbird_query("SELECT ID, TIME_TZ, TIMESTAMP_TZ FROM TZ_TEST ORDER BY ID");
while ($row = fbird_fetch_assoc($q)) {
    echo sprintf("Row %d: TIME=%s, TIMESTAMP=%s\n",
        $row['ID'], $row['TIME_TZ'], $row['TIMESTAMP_TZ']);
}
fbird_free_result($q);

echo "\n=== Test 6: NULL values ===\n";
$stmt = fbird_prepare("INSERT INTO TZ_TEST (ID, TIME_TZ, TIMESTAMP_TZ) VALUES (?, ?, ?)");
fbird_execute($stmt, 5, null, null);
fbird_free_query($stmt);
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_TEST WHERE ID = 5");
$row = fbird_fetch_assoc($q);
echo "ID: " . $row['ID'] . "\n";
echo "TIME_TZ is NULL: " . (is_null($row['TIME_TZ']) ? 'yes' : 'no') . "\n";
echo "TIMESTAMP_TZ is NULL: " . (is_null($row['TIMESTAMP_TZ']) ? 'yes' : 'no') . "\n";
fbird_free_result($q);

fbird_close();
echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Insert with named timezone (Europe/Berlin) ===
ID: 1
TIME_TZ: 10:30:00 Europe/Berlin
TIMESTAMP_TZ: 2025-06-15 10:30:00 Europe/Berlin

=== Test 2: Insert with UTC offset timezone ===
ID: 2
TIME_TZ: 14:45:30 +02:00
TIMESTAMP_TZ: 2025-12-25 14:45:30 +02:00

=== Test 3: Insert with UTC timezone ===
ID: 3
TIME_TZ: 00:00:00 UTC
TIMESTAMP_TZ: 2025-01-01 00:00:00 UTC

=== Test 4: Insert with negative UTC offset ===
ID: 4
TIME_TZ: 08:00:00 -05:00
TIMESTAMP_TZ: 2025-07-04 08:00:00 -05:00

=== Test 5: Verify all rows ===
Row 1: TIME=10:30:00 Europe/Berlin, TIMESTAMP=2025-06-15 10:30:00 Europe/Berlin
Row 2: TIME=14:45:30 +02:00, TIMESTAMP=2025-12-25 14:45:30 +02:00
Row 3: TIME=00:00:00 UTC, TIMESTAMP=2025-01-01 00:00:00 UTC
Row 4: TIME=08:00:00 -05:00, TIMESTAMP=2025-07-04 08:00:00 -05:00

=== Test 6: NULL values ===
ID: 5
TIME_TZ is NULL: yes
TIMESTAMP_TZ is NULL: yes

Done.

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE TZ_TEST");
    @fbird_close($db);
}
?>

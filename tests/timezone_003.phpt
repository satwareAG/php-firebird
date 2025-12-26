--TEST--
Firebird 4.0+ timezone types: UPDATE operations with parameterized binding
--SKIPIF--
<?php
include("skipif.inc");
skip_if_fb_lt(4);
skip_if_fbclient_lt(4);
?>
--FILE--
<?php
/*
 * Test UPDATE operations with parameterized timezone values.
 * This further verifies the binding implementation in fbird_query_bind.c.
 */

require("firebird.inc");

fbird_connect($test_base);

// Create and populate test table
fbird_query("CREATE TABLE TZ_UPDATE_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    TIME_TZ TIME WITH TIME ZONE,
    TIMESTAMP_TZ TIMESTAMP WITH TIME ZONE
)");
fbird_commit();

// Insert initial data
fbird_query("INSERT INTO TZ_UPDATE_TEST VALUES (1, '10:00:00 UTC', '2025-01-01 10:00:00 UTC')");
fbird_query("INSERT INTO TZ_UPDATE_TEST VALUES (2, '11:00:00 UTC', '2025-02-01 11:00:00 UTC')");
fbird_query("INSERT INTO TZ_UPDATE_TEST VALUES (3, '12:00:00 UTC', '2025-03-01 12:00:00 UTC')");
fbird_commit();

echo "=== Initial data ===\n";
$q = fbird_query("SELECT * FROM TZ_UPDATE_TEST ORDER BY ID");
while ($row = fbird_fetch_assoc($q)) {
    echo sprintf("ID=%d: TIME=%s, TIMESTAMP=%s\n",
        $row['ID'], $row['TIME_TZ'], $row['TIMESTAMP_TZ']);
}
fbird_free_result($q);

echo "\n=== Test 1: UPDATE with parameterized timezone values ===\n";
$stmt = fbird_prepare("UPDATE TZ_UPDATE_TEST SET TIME_TZ = ?, TIMESTAMP_TZ = ? WHERE ID = ?");
fbird_execute($stmt, '20:30:00 Europe/Berlin', '2025-12-31 20:30:00 Europe/Berlin', 1);
fbird_free_query($stmt);
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_UPDATE_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($q);
echo "Updated row 1: TIME=" . $row['TIME_TZ'] . ", TIMESTAMP=" . $row['TIMESTAMP_TZ'] . "\n";
fbird_free_result($q);

echo "\n=== Test 2: UPDATE only TIME_TZ with parameter ===\n";
$stmt = fbird_prepare("UPDATE TZ_UPDATE_TEST SET TIME_TZ = ? WHERE ID = ?");
fbird_execute($stmt, '08:15:30 America/Los_Angeles', 2);
fbird_free_query($stmt);
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_UPDATE_TEST WHERE ID = 2");
$row = fbird_fetch_assoc($q);
echo "Updated row 2: TIME=" . $row['TIME_TZ'] . ", TIMESTAMP=" . $row['TIMESTAMP_TZ'] . "\n";
fbird_free_result($q);

echo "\n=== Test 3: UPDATE only TIMESTAMP_TZ with parameter ===\n";
$stmt = fbird_prepare("UPDATE TZ_UPDATE_TEST SET TIMESTAMP_TZ = ? WHERE ID = ?");
fbird_execute($stmt, '2025-07-04 12:00:00 America/New_York', 3);
fbird_free_query($stmt);
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_UPDATE_TEST WHERE ID = 3");
$row = fbird_fetch_assoc($q);
echo "Updated row 3: TIME=" . $row['TIME_TZ'] . ", TIMESTAMP=" . $row['TIMESTAMP_TZ'] . "\n";
fbird_free_result($q);

echo "\n=== Test 4: UPDATE to NULL values ===\n";
$stmt = fbird_prepare("UPDATE TZ_UPDATE_TEST SET TIME_TZ = ?, TIMESTAMP_TZ = ? WHERE ID = ?");
fbird_execute($stmt, null, null, 1);
fbird_free_query($stmt);
fbird_commit();

$q = fbird_query("SELECT * FROM TZ_UPDATE_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($q);
echo "Row 1 after NULL update:\n";
echo "  TIME_TZ is NULL: " . (is_null($row['TIME_TZ']) ? 'yes' : 'no') . "\n";
echo "  TIMESTAMP_TZ is NULL: " . (is_null($row['TIMESTAMP_TZ']) ? 'yes' : 'no') . "\n";
fbird_free_result($q);

echo "\n=== Test 5: Transactional UPDATE with rollback ===\n";
$tr = fbird_trans();
$stmt = fbird_prepare($tr, "UPDATE TZ_UPDATE_TEST SET TIME_TZ = ? WHERE ID = ?");
fbird_execute($stmt, '23:59:59 UTC', 2);
fbird_free_query($stmt);

// Check value before rollback
$q = fbird_query($tr, "SELECT TIME_TZ FROM TZ_UPDATE_TEST WHERE ID = 2");
$row = fbird_fetch_assoc($q);
echo "Before rollback: TIME=" . $row['TIME_TZ'] . "\n";
fbird_free_result($q);

fbird_rollback($tr);

// Check value after rollback
$q = fbird_query("SELECT TIME_TZ FROM TZ_UPDATE_TEST WHERE ID = 2");
$row = fbird_fetch_assoc($q);
echo "After rollback: TIME=" . $row['TIME_TZ'] . "\n";
fbird_free_result($q);

fbird_close();
echo "\nDone.\n";
?>
--EXPECTF--
=== Initial data ===
ID=1: TIME=10:00:00 UTC, TIMESTAMP=2025-01-01 10:00:00 UTC
ID=2: TIME=11:00:00 UTC, TIMESTAMP=2025-02-01 11:00:00 UTC
ID=3: TIME=12:00:00 UTC, TIMESTAMP=2025-03-01 12:00:00 UTC

=== Test 1: UPDATE with parameterized timezone values ===
Updated row 1: TIME=20:30:00 Europe/Berlin, TIMESTAMP=2025-12-31 20:30:00 Europe/Berlin

=== Test 2: UPDATE only TIME_TZ with parameter ===
Updated row 2: TIME=08:15:30 America/Los_Angeles, TIMESTAMP=2025-02-01 11:00:00 UTC

=== Test 3: UPDATE only TIMESTAMP_TZ with parameter ===
Updated row 3: TIME=12:00:00 UTC, TIMESTAMP=2025-07-04 12:00:00 America/New_York

=== Test 4: UPDATE to NULL values ===
Row 1 after NULL update:
  TIME_TZ is NULL: yes
  TIMESTAMP_TZ is NULL: yes

=== Test 5: Transactional UPDATE with rollback ===
Before rollback: TIME=23:59:59 UTC
After rollback: TIME=08:15:30 America/Los_Angeles

Done.

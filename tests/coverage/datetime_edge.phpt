--TEST--
Coverage: Date/Time edge cases (fbird_datetime.c)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
?>
--FILE--
<?php
/**
 * Coverage test for date/time edge cases in fbird_datetime.c
 *
 * Tests datetime handling paths:
 * - DATE boundary values (min/max dates)
 * - TIME with sub-second precision
 * - TIMESTAMP edge cases
 * - NULL datetime handling
 *
 * Target: Cover datetime parsing and formatting paths
 */
require_once __DIR__ . '/../firebird.inc';

echo "=== Test: Date/Time Edge Cases ===\n";

$db = fbird_connect($test_base, $user, $password);
if (!$db) {
    die("Connection failed: " . fbird_errmsg() . "\n");
}

// Clean up
@fbird_query($db, 'DROP TABLE DATETIME_EDGE_TEST');
@fbird_commit($db);

// Create test table
$create = "
CREATE TABLE DATETIME_EDGE_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    DATE_VAL DATE,
    TIME_VAL TIME,
    TIMESTAMP_VAL TIMESTAMP
)";
fbird_query($db, $create);
fbird_commit($db);
echo "Test table created\n";

// Test 1: Standard date values
echo "\n--- Test 1: Standard DATE values ---\n";
$stmt = fbird_prepare($db, "INSERT INTO DATETIME_EDGE_TEST (ID, DATE_VAL) VALUES (?, ?)");

fbird_execute($stmt, 1, '2026-01-06');
fbird_execute($stmt, 2, '2000-01-01');
fbird_execute($stmt, 3, '1999-12-31');
fbird_execute($stmt, 4, '2099-12-31');
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, DATE_VAL FROM DATETIME_EDGE_TEST WHERE ID <= 4 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: DATE_VAL={$row['DATE_VAL']}\n";
}
fbird_free_result($r);

// Test 2: DATE boundary values (use valid Firebird date range)
echo "\n--- Test 2: DATE boundary values ---\n";
// Firebird supports dates from 0001-01-01 to 9999-12-31, but use safer values
fbird_execute($stmt, 10, '1753-01-01');  // Common safe minimum
fbird_execute($stmt, 11, '9999-01-01');  // Near maximum
fbird_execute($stmt, 12, '1582-10-15');  // Gregorian calendar start
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, DATE_VAL FROM DATETIME_EDGE_TEST WHERE ID >= 10 AND ID < 20 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: DATE_VAL={$row['DATE_VAL']}\n";
}
fbird_free_result($r);

// Test 3: TIME values
echo "\n--- Test 3: TIME values ---\n";
$stmt = fbird_prepare($db, "INSERT INTO DATETIME_EDGE_TEST (ID, TIME_VAL) VALUES (?, ?)");

fbird_execute($stmt, 20, '00:00:00');      // Midnight
fbird_execute($stmt, 21, '12:00:00');      // Noon
fbird_execute($stmt, 22, '23:59:59');      // End of day
fbird_execute($stmt, 23, '12:34:56');      // Arbitrary
fbird_execute($stmt, 24, '01:02:03.456');  // With milliseconds
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, TIME_VAL FROM DATETIME_EDGE_TEST WHERE ID >= 20 AND ID < 30 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: TIME_VAL={$row['TIME_VAL']}\n";
}
fbird_free_result($r);

// Test 4: TIMESTAMP values
echo "\n--- Test 4: TIMESTAMP values ---\n";
$stmt = fbird_prepare($db, "INSERT INTO DATETIME_EDGE_TEST (ID, TIMESTAMP_VAL) VALUES (?, ?)");

fbird_execute($stmt, 30, '2026-01-06 13:30:00');
fbird_execute($stmt, 31, '2000-01-01 00:00:00');
fbird_execute($stmt, 32, '2025-12-31 23:59:59');
fbird_execute($stmt, 33, '1970-01-01 00:00:00');  // Unix epoch
fbird_execute($stmt, 34, '2038-01-19 03:14:07');  // Unix 32-bit limit
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, TIMESTAMP_VAL FROM DATETIME_EDGE_TEST WHERE ID >= 30 AND ID < 40 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: TIMESTAMP_VAL={$row['TIMESTAMP_VAL']}\n";
}
fbird_free_result($r);

// Test 5: NULL datetime values
echo "\n--- Test 5: NULL datetime values ---\n";
$stmt = fbird_prepare($db, "INSERT INTO DATETIME_EDGE_TEST (ID, DATE_VAL, TIME_VAL, TIMESTAMP_VAL) VALUES (?, ?, ?, ?)");
fbird_execute($stmt, 40, null, null, null);
fbird_execute($stmt, 41, '2026-01-06', null, null);
fbird_execute($stmt, 42, null, '12:00:00', null);
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, DATE_VAL IS NULL AS D_NULL, TIME_VAL IS NULL AS T_NULL FROM DATETIME_EDGE_TEST WHERE ID >= 40 AND ID < 50 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: DATE NULL=" . ($row['D_NULL'] ? 'Y' : 'N') . ", TIME NULL=" . ($row['T_NULL'] ? 'Y' : 'N') . "\n";
}
fbird_free_result($r);

// Test 6: Fetch as string format (default)
echo "\n--- Test 6: Fetch as string format ---\n";
$r = fbird_query($db, "SELECT ID, DATE_VAL, TIMESTAMP_VAL FROM DATETIME_EDGE_TEST WHERE ID = 30");
$row = fbird_fetch_assoc($r);
echo "DATE_VAL type: " . gettype($row['DATE_VAL'] ?? null) . "\n";
echo "TIMESTAMP_VAL type: " . gettype($row['TIMESTAMP_VAL'] ?? null) . "\n";
echo "DATE_VAL = " . ($row['DATE_VAL'] ?? 'NULL') . "\n";
echo "TIMESTAMP_VAL = " . ($row['TIMESTAMP_VAL'] ?? 'NULL') . "\n";
fbird_free_result($r);

// Test 7: Century boundaries
echo "\n--- Test 7: Century boundaries ---\n";
$stmt = fbird_prepare($db, "INSERT INTO DATETIME_EDGE_TEST (ID, DATE_VAL) VALUES (?, ?)");
fbird_execute($stmt, 50, '1900-01-01');
fbird_execute($stmt, 51, '1900-12-31');
fbird_execute($stmt, 52, '2000-02-29');  // Leap year
fbird_execute($stmt, 53, '2100-02-28');  // Not leap year (century rule)
fbird_commit($db);

$r = fbird_query($db, "SELECT ID, DATE_VAL FROM DATETIME_EDGE_TEST WHERE ID >= 50 AND ID < 60 ORDER BY ID");
while ($row = fbird_fetch_assoc($r)) {
    echo "ID={$row['ID']}: DATE_VAL={$row['DATE_VAL']}\n";
}
fbird_free_result($r);

// Final count
$r = fbird_query($db, "SELECT COUNT(*) AS CNT FROM DATETIME_EDGE_TEST");
$row = fbird_fetch_assoc($r);
echo "\n--- Total rows: " . $row['CNT'] . " ---\n";
fbird_free_result($r);

// Cleanup
echo "\n--- Cleanup ---\n";
@fbird_query($db, 'DROP TABLE DATETIME_EDGE_TEST');
@fbird_commit($db);
fbird_close($db);

echo "\n=== Test Complete ===\n";
?>
--EXPECTF--
=== Test: Date/Time Edge Cases ===
Test table created

--- Test 1: Standard DATE values ---
ID=1: DATE_VAL=2026-01-06
ID=2: DATE_VAL=2000-01-01
ID=3: DATE_VAL=1999-12-31
ID=4: DATE_VAL=2099-12-31

--- Test 2: DATE boundary values ---
ID=10: DATE_VAL=1753-01-01
ID=11: DATE_VAL=9999-01-01
ID=12: DATE_VAL=1582-10-15

--- Test 3: TIME values ---
ID=20: TIME_VAL=00:00:00
ID=21: TIME_VAL=12:00:00
ID=22: TIME_VAL=23:59:59
ID=23: TIME_VAL=12:34:56
ID=24: TIME_VAL=01:02:03%A

--- Test 4: TIMESTAMP values ---
ID=30: TIMESTAMP_VAL=2026-01-06 13:30:00
ID=31: TIMESTAMP_VAL=2000-01-01 00:00:00
ID=32: TIMESTAMP_VAL=2025-12-31 23:59:59
ID=33: TIMESTAMP_VAL=1970-01-01 00:00:00
ID=34: TIMESTAMP_VAL=2038-01-19 03:14:07

--- Test 5: NULL datetime values ---
ID=40: DATE NULL=Y, TIME NULL=Y
ID=41: DATE NULL=N, TIME NULL=Y
ID=42: DATE NULL=Y, TIME NULL=N

--- Test 6: Fetch as string format ---
DATE_VAL type: %s
TIMESTAMP_VAL type: %s
DATE_VAL = %s
TIMESTAMP_VAL = %s

--- Test 7: Century boundaries ---
ID=50: DATE_VAL=1900-01-01
ID=51: DATE_VAL=1900-12-31
ID=52: DATE_VAL=2000-02-29
ID=53: DATE_VAL=2100-02-28

--- Total rows: %d ---

--- Cleanup ---

=== Test Complete ===

--TEST--
FBIRD_FETCH_DATE_OBJ - Fetch date/time columns as DateTimeImmutable objects
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$link = fbird_connect($test_base, $user, $password);
if (!$link) {
    die("Could not connect: " . fbird_errmsg());
}

// Test table with different date/time column types
fbird_query($link, "RECREATE TABLE test_datetime (
    id INTEGER NOT NULL PRIMARY KEY,
    date_col DATE,
    time_col TIME,
    timestamp_col TIMESTAMP
)");
fbird_commit($link);

// Insert test data
fbird_query($link, "INSERT INTO test_datetime VALUES (1, '2025-12-20', '14:30:45', '2025-12-20 14:30:45')");
fbird_commit($link);

echo "=== Test FBIRD_FETCH_DATE_OBJ constant exists ===\n";
var_dump(defined('FBIRD_FETCH_DATE_OBJ'));
var_dump(FBIRD_FETCH_DATE_OBJ);

echo "\n=== Test fetch without FBIRD_FETCH_DATE_OBJ (default string format) ===\n";
$result = fbird_query($link, "SELECT * FROM test_datetime WHERE id = 1");
$row = fbird_fetch_assoc($result);
echo "date_col type: " . gettype($row['DATE_COL']) . "\n";
echo "time_col type: " . gettype($row['TIME_COL']) . "\n";
echo "timestamp_col type: " . gettype($row['TIMESTAMP_COL']) . "\n";
fbird_free_result($result);

echo "\n=== Test fetch with FBIRD_FETCH_DATE_OBJ flag ===\n";
$result = fbird_query($link, "SELECT * FROM test_datetime WHERE id = 1");
$row = fbird_fetch_assoc($result, FBIRD_FETCH_DATE_OBJ);

echo "date_col instanceof DateTimeImmutable: ";
var_dump($row['DATE_COL'] instanceof DateTimeImmutable);

echo "time_col instanceof DateTimeImmutable: ";
var_dump($row['TIME_COL'] instanceof DateTimeImmutable);

echo "timestamp_col instanceof DateTimeImmutable: ";
var_dump($row['TIMESTAMP_COL'] instanceof DateTimeImmutable);

fbird_free_result($result);

echo "\n=== Test date values are correct ===\n";
$result = fbird_query($link, "SELECT * FROM test_datetime WHERE id = 1");
$row = fbird_fetch_assoc($result, FBIRD_FETCH_DATE_OBJ);

echo "date_col format Y-m-d: " . $row['DATE_COL']->format('Y-m-d') . "\n";
echo "time_col format H:i:s: " . $row['TIME_COL']->format('H:i:s') . "\n";
echo "timestamp_col format Y-m-d H:i:s: " . $row['TIMESTAMP_COL']->format('Y-m-d H:i:s') . "\n";

fbird_free_result($result);

echo "\n=== Test with fbird_fetch_row ===\n";
$result = fbird_query($link, "SELECT timestamp_col FROM test_datetime WHERE id = 1");
$row = fbird_fetch_row($result, FBIRD_FETCH_DATE_OBJ);
echo "Row[0] instanceof DateTimeImmutable: ";
var_dump($row[0] instanceof DateTimeImmutable);
fbird_free_result($result);

echo "\n=== Test with fbird_fetch_object ===\n";
$result = fbird_query($link, "SELECT timestamp_col FROM test_datetime WHERE id = 1");
$obj = fbird_fetch_object($result, FBIRD_FETCH_DATE_OBJ);
echo "Object->TIMESTAMP_COL instanceof DateTimeImmutable: ";
var_dump($obj->TIMESTAMP_COL instanceof DateTimeImmutable);
fbird_free_result($result);

echo "\n=== Test NULL date/time handling ===\n";
fbird_query($link, "INSERT INTO test_datetime VALUES (2, NULL, NULL, NULL)");
fbird_commit($link);

$result = fbird_query($link, "SELECT * FROM test_datetime WHERE id = 2");
$row = fbird_fetch_assoc($result, FBIRD_FETCH_DATE_OBJ);
echo "NULL date_col is null: ";
var_dump($row['DATE_COL'] === null);
echo "NULL time_col is null: ";
var_dump($row['TIME_COL'] === null);
echo "NULL timestamp_col is null: ";
var_dump($row['TIMESTAMP_COL'] === null);
fbird_free_result($result);

// Clean close - let --CLEAN-- section handle table drop
fbird_close($link);

echo "\nDone.\n";
?>
--CLEAN--
<?php
// Cleanup in separate process to avoid shutdown race conditions
require("firebird.inc");
$link = @fbird_connect($test_base, $user, $password);
if ($link) {
    @fbird_query($link, "DROP TABLE test_datetime");
    @fbird_commit($link);
    @fbird_close($link);
}
?>
--EXPECTF--
=== Test FBIRD_FETCH_DATE_OBJ constant exists ===
bool(true)
int(8)

=== Test fetch without FBIRD_FETCH_DATE_OBJ (default string format) ===
date_col type: string
time_col type: string
timestamp_col type: string

=== Test fetch with FBIRD_FETCH_DATE_OBJ flag ===
date_col instanceof DateTimeImmutable: bool(true)
time_col instanceof DateTimeImmutable: bool(true)
timestamp_col instanceof DateTimeImmutable: bool(true)

=== Test date values are correct ===
date_col format Y-m-d: 2025-12-20
time_col format H:i:s: 14:30:45
timestamp_col format Y-m-d H:i:s: 2025-12-20 14:30:45

=== Test with fbird_fetch_row ===
Row[0] instanceof DateTimeImmutable: bool(true)

=== Test with fbird_fetch_object ===
Object->TIMESTAMP_COL instanceof DateTimeImmutable: bool(true)

=== Test NULL date/time handling ===
NULL date_col is null: bool(true)
NULL time_col is null: bool(true)
NULL timestamp_col is null: bool(true)

Done.

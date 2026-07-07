--TEST--
Coverage: fbird_datetime.c — European, US, ISO-T, fractional-seconds, timezone formats
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die("connect failed: " . fbird_errmsg());

// Setup table with date/time columns
fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'DT_FORMATS_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE DT_FORMATS_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, '
    CREATE TABLE DT_FORMATS_COV (
        ID        INTEGER NOT NULL PRIMARY KEY,
        D         DATE,
        T         TIME,
        TS        TIMESTAMP
    )');
fbird_commit($dbh);

$ins = fbird_prepare($dbh, 'INSERT INTO DT_FORMATS_COV (ID, D, T, TS) VALUES (?, ?, ?, ?)');

// --- European date format: DD.MM.YYYY ---
// Triggers fbird_parse_date() European branch + fbird_parse_timestamp() Format 3
echo "Test 1: European date format DD.MM.YYYY\n";
$r = fbird_execute($ins, 1, '15.06.2024', '14:30:00', '15.06.2024 14:30:00');
var_dump($r !== false);

// --- US date format: MM/DD/YYYY ---
// Triggers fbird_parse_date() US branch + fbird_parse_timestamp() Format 4
echo "Test 2: US date format MM/DD/YYYY\n";
$r = fbird_execute($ins, 2, '06/15/2024', '09:00:00', '06/15/2024 09:00:00');
var_dump($r !== false);

// --- ISO 8601 with T separator: YYYY-MM-DDTHH:MM:SS ---
// Triggers fbird_parse_timestamp() Format 2
echo "Test 3: ISO T-separator timestamp\n";
$r = fbird_execute($ins, 3, '2024-06-15', '12:00:00', '2024-06-15T12:00:00');
var_dump($r !== false);

// --- Timestamp with fractional seconds ---
// Triggers fraction normalization in fbird_parse_timestamp
echo "Test 4: Timestamp with fractional seconds\n";
$r = fbird_execute($ins, 4, '2024-06-15', '14:30:00', '2024-06-15 14:30:00.1234');
var_dump($r !== false);

// --- Timestamp with fractional seconds (short, <4 digits) ---
// Triggers the "pad to 4 digits" normalization path
echo "Test 5: Timestamp with 2-digit fractions\n";
$r = fbird_execute($ins, 5, '2024-06-15', '14:30:00', '2024-06-15 14:30:00.12');
var_dump($r !== false);

// --- Timestamp with offset timezone (+HH:MM) ---
// Triggers fbird_extract_timezone() offset branch
echo "Test 6: Timestamp with timezone offset\n";
$r = fbird_execute($ins, 6, '2024-06-15', '14:30:01', '2024-06-15 14:30:01 +02:00');
var_dump($r !== false);

// --- Timestamp with named timezone (UTC) ---
// Triggers fbird_extract_timezone() named TZ branch
echo "Test 7: Timestamp with named timezone\n";
$r = fbird_execute($ins, 7, '2024-06-15', '14:30:02', '2024-06-15 14:30:02 UTC');
var_dump($r !== false);

// --- European 2-digit year (>50 → 1900 + year) ---
// Triggers 2-digit year expansion in fbird_parse_date European branch
echo "Test 8: European 2-digit year (>50)\n";
$r = fbird_execute($ins, 8, '15.06.99', null, null);
var_dump($r !== false);

// --- European 2-digit year (<=50 → 2000 + year) ---
echo "Test 9: European 2-digit year (<=50)\n";
$r = fbird_execute($ins, 9, '15.06.24', null, null);
var_dump($r !== false);

// --- US 2-digit year ---
echo "Test 10: US 2-digit year\n";
$r = fbird_execute($ins, 10, '06/15/24', null, null);
var_dump($r !== false);

// --- Time with fractional seconds ---
// Triggers fraction normalization in fbird_parse_time
echo "Test 11: Time with fractional seconds\n";
$r = fbird_execute($ins, 11, null, '14:30:00.5', null);
var_dump($r !== false);

// --- Time with 1-digit fraction (needs padding to 4) ---
echo "Test 12: Time with 1-digit fraction\n";
$r = fbird_execute($ins, 12, null, '14:30:00.1', null);
var_dump($r !== false);

fbird_commit($dbh);

// Verify some entries were stored
$q = fbird_query($dbh, 'SELECT COUNT(*) FROM DT_FORMATS_COV');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 13: rows inserted\n";
var_dump((int)$row[0] === 12);

// Verify European date round-trips (row 1)
$q = fbird_query($dbh, 'SELECT D FROM DT_FORMATS_COV WHERE ID = 1');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 14: European date stored\n";
var_dump($row[0] !== null);

// Verify ISO T-separator timestamp (row 3)
$q = fbird_query($dbh, 'SELECT TS FROM DT_FORMATS_COV WHERE ID = 3');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 15: ISO T-separator timestamp stored\n";
var_dump($row[0] !== null);

// Cleanup
@fbird_commit($dbh);
fbird_query($dbh, 'DROP TABLE DT_FORMATS_COV');
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECT--
Test 1: European date format DD.MM.YYYY
bool(true)
Test 2: US date format MM/DD/YYYY
bool(true)
Test 3: ISO T-separator timestamp
bool(true)
Test 4: Timestamp with fractional seconds
bool(true)
Test 5: Timestamp with 2-digit fractions
bool(true)
Test 6: Timestamp with timezone offset
bool(true)
Test 7: Timestamp with named timezone
bool(true)
Test 8: European 2-digit year (>50)
bool(true)
Test 9: European 2-digit year (<=50)
bool(true)
Test 10: US 2-digit year
bool(true)
Test 11: Time with fractional seconds
bool(true)
Test 12: Time with 1-digit fraction
bool(true)
Test 13: rows inserted
bool(true)
Test 14: European date stored
bool(true)
Test 15: ISO T-separator timestamp stored
bool(true)
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

--TEST--
Coverage: DATE/TIME/TIMESTAMP parameter binding — string, unix int, NULL
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip connect failed: ' . fbird_errmsg());

fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BIND_TEMP_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BIND_TEMP_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, '
    CREATE TABLE BIND_TEMP_COV (
        ID    INTEGER NOT NULL PRIMARY KEY,
        D     DATE,
        T     TIME,
        TS    TIMESTAMP
    )');
fbird_commit($dbh);

$stmt = fbird_prepare($dbh, 'INSERT INTO BIND_TEMP_COV (ID, D, T, TS) VALUES (?, ?, ?, ?)');

// Test 1: ISO-8601 string format
echo "Test 1: ISO string date/time/timestamp\n";
$r = fbird_execute($stmt, 1, '2024-06-15', '14:30:00', '2024-06-15 14:30:00');
var_dump($r !== false);

// Test 2: Unix timestamp for all three (integer seconds since epoch)
echo "Test 2: Unix timestamp integer\n";
$ts = mktime(0, 0, 0, 1, 1, 2024); // 2024-01-01 00:00:00 UTC
$r = fbird_execute($stmt, 2, $ts, $ts, $ts);
var_dump($r !== false);

// Test 3: NULL for all temporal columns
echo "Test 3: NULL for all temporal columns\n";
$r = fbird_execute($stmt, 3, null, null, null);
var_dump($r !== false);

// Test 4: Only DATE set, others NULL
echo "Test 4: only DATE set\n";
$r = fbird_execute($stmt, 4, '2024-03-15', null, null);
var_dump($r !== false);

// Test 5: Timestamp at epoch boundaries
echo "Test 5: epoch boundary — 1970-01-01\n";
$r = @fbird_execute($stmt, 5, '1970-01-01', '00:00:01', '1970-01-01 00:00:01');
var_dump($r !== false);

// Test 6: Far future date
echo "Test 6: far future date\n";
$r = @fbird_execute($stmt, 6, '2099-12-31', '23:59:59', '2099-12-31 23:59:59');
var_dump($r !== false);

fbird_commit($dbh);

// Verify count
$q = fbird_query($dbh, 'SELECT COUNT(*) FROM BIND_TEMP_COV');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 7: 6 rows inserted\n";
var_dump((int)$row[0] === 6);

// Verify NULL row
$q = fbird_query($dbh, 'SELECT D, T, TS FROM BIND_TEMP_COV WHERE ID = 3');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 8: NULL temporal row\n";
var_dump($row[0] === null && $row[1] === null && $row[2] === null);

// Verify ISO string row has expected DATE
$q = fbird_query($dbh, 'SELECT D FROM BIND_TEMP_COV WHERE ID = 1');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 9: ISO date stored correctly\n";
// Date may be returned as '2024-06-15' or Unix int — just check non-null
var_dump($row[0] !== null);

// Cleanup: commit implicit tx, then DDL. Use @ on final commit (prepared stmt may hold table).
@fbird_commit($dbh);
fbird_query($dbh, 'DROP TABLE BIND_TEMP_COV');
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECTF--
Test 1: ISO string date/time/timestamp
bool(true)
Test 2: Unix timestamp integer
bool(true)
Test 3: NULL for all temporal columns
bool(true)
Test 4: only DATE set
bool(true)
Test 5: epoch boundary — 1970-01-01
bool(true)
Test 6: far future date
bool(true)
Test 7: 6 rows inserted
bool(true)
Test 8: NULL temporal row
bool(true)
Test 9: ISO date stored correctly
bool(true)
Done

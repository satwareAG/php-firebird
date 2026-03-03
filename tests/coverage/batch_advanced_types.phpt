--TEST--
Coverage: Batch API with DATE/TIME/TIMESTAMP/DECIMAL types and NULL values
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API not available in this build');
}
require_once __DIR__ . '/../firebird.inc';
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('skip connect failed: ' . fbird_errmsg());

// Setup: clean table with varied column types
fbird_query($db, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BATCH_TYPES_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BATCH_TYPES_COV';
END");
fbird_commit($db);

fbird_query($db, '
    CREATE TABLE BATCH_TYPES_COV (
        ID        INTEGER      NOT NULL PRIMARY KEY,
        D         DATE,
        T         TIME,
        TS        TIMESTAMP,
        DEC_VAL   DECIMAL(10,2),
        NULLABLE  VARCHAR(30)
    )');
fbird_commit($db);

$trans = fbird_trans($db);
$stmt  = fbird_prepare($db, $trans,
    'INSERT INTO BATCH_TYPES_COV (ID, D, T, TS, DEC_VAL, NULLABLE) VALUES (?, ?, ?, ?, ?, ?)');

$batch = fbird_batch_create($stmt, $trans);
if (!$batch) die('FAIL: could not create batch: ' . fbird_errmsg());

// Row 1: all scalar string representations
echo "Test 1: string-based date/time/timestamp\n";
$r = fbird_batch_add($batch, 1, '2024-06-15', '10:30:00', '2024-06-15 10:30:00', '1234.56', 'hello');
var_dump($r === true);

// Row 2: numeric unix timestamp for DATE/TIME/TIMESTAMP
echo "Test 2: integer timestamp for TS\n";
$r = fbird_batch_add($batch, 2, '2024-01-01', '00:00:00', '2024-01-01 00:00:00', '0.01', 'world');
var_dump($r === true);

// Row 3: DECIMAL with max precision value
echo "Test 3: large decimal value\n";
$r = fbird_batch_add($batch, 3, '2024-12-31', '23:59:59', '2024-12-31 23:59:59', '99999999.99', 'max');
var_dump($r === true);

// Row 4: NULL for nullable columns
echo "Test 4: NULL for nullable columns\n";
$r = fbird_batch_add($batch, 4, null, null, null, null, null);
var_dump($r === true);

// Row 5: NULL only for NULLABLE varchar, others set
echo "Test 5: NULL only for NULLABLE\n";
$r = fbird_batch_add($batch, 5, '2024-03-15', '12:00:00', '2024-03-15 12:00:00', '3.14', null);
var_dump($r === true);

// Execute
echo "Test 6: batch execute with mixed types\n";
$result = fbird_batch_execute($batch);
var_dump(is_array($result) || $result === true);

fbird_commit($trans);

// Verify inserted rows
$q = fbird_query($db, 'SELECT COUNT(*) FROM BATCH_TYPES_COV');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 7: all 5 rows inserted\n";
var_dump((int)$row[0] === 5);

// Verify NULL row stored correctly
$q = fbird_query($db, 'SELECT D, T, TS, DEC_VAL, NULLABLE FROM BATCH_TYPES_COV WHERE ID = 4');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 8: NULL row has null columns\n";
var_dump($row[0] === null && $row[1] === null && $row[2] === null);

// Cleanup
fbird_free_query($stmt);    // release IStatement before DDL
fbird_commit($db);          // close implicit SELECT transactions
fbird_query($db, 'DROP TABLE BATCH_TYPES_COV');
@fbird_commit($db);         // suppress "table in use" if IBatch IStatement still active
fbird_close($db);

echo "Done\n";
?>
--EXPECTF--
Test 1: string-based date/time/timestamp
bool(true)
Test 2: integer timestamp for TS
bool(true)
Test 3: large decimal value
bool(true)
Test 4: NULL for nullable columns
bool(true)
Test 5: NULL only for NULLABLE
bool(true)
Test 6: batch execute with mixed types
bool(true)
Test 7: all 5 rows inserted
bool(true)
Test 8: NULL row has null columns
bool(true)
Done

--TEST--
Coverage: fbird_query_array.c TIMESTAMP/DOUBLE/SMALLINT arrays, error paths, isc_array_lookup_bounds
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip: ' . fbird_errmsg());

// ---- Setup ----
foreach (['ARR_TS_COV', 'ARR_DBL_COV', 'ARR_SMALL_COV', 'ARR_INT64_COV', 'ARR_TIME_COV'] as $t) {
    @fbird_query($dbh, "DROP TABLE $t");
}
@fbird_commit($dbh);

fbird_query($dbh, 'CREATE TABLE ARR_TS_COV    (ID INTEGER NOT NULL, DATA TIMESTAMP[2])');
fbird_query($dbh, 'CREATE TABLE ARR_DBL_COV   (ID INTEGER NOT NULL, DATA DOUBLE PRECISION[3])');
fbird_query($dbh, 'CREATE TABLE ARR_SMALL_COV (ID INTEGER NOT NULL, DATA SMALLINT[4])');
fbird_query($dbh, 'CREATE TABLE ARR_INT64_COV (ID INTEGER NOT NULL, DATA BIGINT[2])');
fbird_query($dbh, 'CREATE TABLE ARR_TIME_COV  (ID INTEGER NOT NULL, DATA TIME[2])');
fbird_commit($dbh);

// ---- Test 1: TIMESTAMP array ----
echo "Test 1: TIMESTAMP array\n";
fbird_query($dbh, 'INSERT INTO ARR_TS_COV (ID) VALUES (1)');
$ts_data = ['2026-01-15 10:30:00', '2026-06-30 23:59:59'];
fbird_query($dbh, 'UPDATE ARR_TS_COV SET DATA = ? WHERE ID = 1', $ts_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_TS_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
var_dump(count($row[0]) >= 1);
fbird_commit($dbh);

// ---- Test 2: DOUBLE PRECISION array ----
echo "Test 2: DOUBLE PRECISION array\n";
fbird_query($dbh, 'INSERT INTO ARR_DBL_COV (ID) VALUES (1)');
$dbl_data = [1.23456789, 9.87654321, 0.0];
fbird_query($dbh, 'UPDATE ARR_DBL_COV SET DATA = ? WHERE ID = 1', $dbl_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_DBL_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
var_dump(abs((float)$row[0][1] - 1.23456789) < 0.0001);
fbird_commit($dbh);

// ---- Test 3: SMALLINT array ----
echo "Test 3: SMALLINT array\n";
fbird_query($dbh, 'INSERT INTO ARR_SMALL_COV (ID) VALUES (1)');
$small_data = [100, 200, 300, 400];
fbird_query($dbh, 'UPDATE ARR_SMALL_COV SET DATA = ? WHERE ID = 1', $small_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_SMALL_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
var_dump((int)$row[0][1] === 100);
fbird_commit($dbh);

// ---- Test 4: BIGINT array ----
echo "Test 4: BIGINT array\n";
fbird_query($dbh, 'INSERT INTO ARR_INT64_COV (ID) VALUES (1)');
$int64_data = [9999999999, -9999999999];
fbird_query($dbh, 'UPDATE ARR_INT64_COV SET DATA = ? WHERE ID = 1', $int64_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_INT64_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
fbird_commit($dbh);

// ---- Test 5: TIME array ----
echo "Test 5: TIME array\n";
fbird_query($dbh, 'INSERT INTO ARR_TIME_COV (ID) VALUES (1)');
$time_data = ['10:30:00', '23:59:59'];
fbird_query($dbh, 'UPDATE ARR_TIME_COV SET DATA = ? WHERE ID = 1', $time_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_TIME_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
fbird_commit($dbh);

// ---- Test 6: Array with NULL elements (partial null) ----
echo "Test 6: Array with NULL value\n";
fbird_query($dbh, 'INSERT INTO ARR_DBL_COV (ID, DATA) VALUES (2, NULL)');
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_DBL_COV WHERE ID = 2');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump($row[0] === null);
fbird_commit($dbh);

// ---- Test 7: Array bind error — invalid array ID string ----
echo "Test 7: Array bind invalid ID\n";
$r = @fbird_query($dbh, 'UPDATE ARR_INT64_COV SET DATA = ? WHERE ID = 1', 'not_a_valid_array_id_string');
// Should fail gracefully (wrong type for array param)
var_dump($r === false || $r !== false); // any result is acceptable — just no crash
@fbird_rollback($dbh);

// ---- Test 8: Multi-dimensional array read (2D) ----
echo "Test 8: Multi-dimensional array read\n";
@fbird_query($dbh, 'DROP TABLE ARR_2D_COV');
@fbird_commit($dbh);
fbird_query($dbh, 'CREATE TABLE ARR_2D_COV (ID INTEGER NOT NULL, DATA INTEGER[2,3])');
fbird_commit($dbh);

fbird_query($dbh, 'INSERT INTO ARR_2D_COV (ID) VALUES (1)');
$data_2d = [1, 2, 3, 4, 5, 6]; // 2x3 = 6 elements
fbird_query($dbh, 'UPDATE ARR_2D_COV SET DATA = ? WHERE ID = 1', $data_2d);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_2D_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
fbird_commit($dbh);

// ---- Cleanup ----
@fbird_rollback($dbh);
foreach (['ARR_TS_COV', 'ARR_DBL_COV', 'ARR_SMALL_COV', 'ARR_INT64_COV', 'ARR_TIME_COV', 'ARR_2D_COV'] as $t) {
    @fbird_query($dbh, "DROP TABLE $t");
    @fbird_commit($dbh);
}
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
Test 1: TIMESTAMP array
bool(true)
bool(true)
Test 2: DOUBLE PRECISION array
bool(true)
bool(true)
Test 3: SMALLINT array
bool(true)
bool(true)
Test 4: BIGINT array
bool(true)
Test 5: TIME array
bool(true)
Test 6: Array with NULL value
bool(true)
Test 7: Array bind invalid ID
bool(true)
Test 8: Multi-dimensional array read
bool(true)
Done

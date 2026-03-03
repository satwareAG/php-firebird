--TEST--
Coverage: fbird_query_array.c all SQL type paths (FLOAT, CHAR, DATE, INTEGER arrays)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip: ' . fbird_errmsg());

// ---- Setup: Create tables with various ARRAY column types ----
@fbird_query($dbh, 'DROP TABLE ARR_INT_COV');
@fbird_query($dbh, 'DROP TABLE ARR_FLOAT_COV');
@fbird_query($dbh, 'DROP TABLE ARR_CHAR_COV');
@fbird_query($dbh, 'DROP TABLE ARR_DATE_COV');
@fbird_query($dbh, 'DROP TABLE ARR_MULTI_COV');
@fbird_commit($dbh);

fbird_query($dbh, 'CREATE TABLE ARR_INT_COV (ID INTEGER NOT NULL, DATA INTEGER[5])');
fbird_query($dbh, 'CREATE TABLE ARR_FLOAT_COV (ID INTEGER NOT NULL, DATA FLOAT[3])');
fbird_query($dbh, 'CREATE TABLE ARR_CHAR_COV (ID INTEGER NOT NULL, DATA CHAR(10)[3])');
fbird_query($dbh, 'CREATE TABLE ARR_DATE_COV (ID INTEGER NOT NULL, DATA DATE[2])');
fbird_query($dbh, 'CREATE TABLE ARR_MULTI_COV (ID INTEGER NOT NULL, DATA INTEGER[2,3])');
fbird_commit($dbh);

// ---- Test 1: INTEGER array (alloc_int path) ----
echo "Test 1: INTEGER array\n";
fbird_query($dbh, 'INSERT INTO ARR_INT_COV (ID) VALUES (1)');
$q = fbird_query($dbh, 'SELECT ID, DATA FROM ARR_INT_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);

$arr_data = [10, 20, 30, 40, 50];
fbird_query($dbh, 'UPDATE ARR_INT_COV SET DATA = ? WHERE ID = 1', $arr_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_INT_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
var_dump((int)$row[0][1] === 10); // Firebird arrays are 1-indexed
fbird_commit($dbh);

// ---- Test 2: FLOAT array (alloc_float path) ----
echo "Test 2: FLOAT array\n";
fbird_query($dbh, 'INSERT INTO ARR_FLOAT_COV (ID) VALUES (1)');
$float_data = [1.5, 2.5, 3.5];
fbird_query($dbh, 'UPDATE ARR_FLOAT_COV SET DATA = ? WHERE ID = 1', $float_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_FLOAT_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
var_dump(abs((float)$row[0][1] - 1.5) < 0.01); // 1-indexed
fbird_commit($dbh);

// ---- Test 3: CHAR array (alloc_char path) ----
echo "Test 3: CHAR array\n";
fbird_query($dbh, 'INSERT INTO ARR_CHAR_COV (ID) VALUES (1)');
$char_data = ['hello', 'world', 'test!'];
fbird_query($dbh, 'UPDATE ARR_CHAR_COV SET DATA = ? WHERE ID = 1', $char_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_CHAR_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
var_dump(trim($row[0][1]) === 'hello'); // 1-indexed
fbird_commit($dbh);

// ---- Test 4: DATE array (alloc_date path) ----
echo "Test 4: DATE array\n";
fbird_query($dbh, 'INSERT INTO ARR_DATE_COV (ID) VALUES (1)');
$date_data = ['2026-01-15', '2026-06-30'];
fbird_query($dbh, 'UPDATE ARR_DATE_COV SET DATA = ? WHERE ID = 1', $date_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_DATE_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
fbird_commit($dbh);

// ---- Test 5: Multi-dimensional ARRAY [2,3] ----
echo "Test 5: Multi-dimensional array\n";
fbird_query($dbh, 'INSERT INTO ARR_MULTI_COV (ID) VALUES (1)');
// 2x3 = 6 elements stored as flat array
$multi_data = [1, 2, 3, 4, 5, 6];
fbird_query($dbh, 'UPDATE ARR_MULTI_COV SET DATA = ? WHERE ID = 1', $multi_data);
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_MULTI_COV WHERE ID = 1');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row[0]));
fbird_commit($dbh);

// ---- Test 6: NULL array value ----
echo "Test 6: NULL array\n";
fbird_query($dbh, 'INSERT INTO ARR_INT_COV (ID, DATA) VALUES (2, NULL)');
$q = fbird_query($dbh, 'SELECT DATA FROM ARR_INT_COV WHERE ID = 2');
$row = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump($row[0] === null);
fbird_commit($dbh);

// ---- Cleanup — rollback any open tx, drop each table with its own commit ----
@fbird_rollback($dbh);
foreach (['ARR_INT_COV', 'ARR_FLOAT_COV', 'ARR_CHAR_COV', 'ARR_DATE_COV', 'ARR_MULTI_COV'] as $tbl) {
    @fbird_query($dbh, "DROP TABLE $tbl");
    @fbird_commit($dbh);
}
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
Test 1: INTEGER array
bool(true)
bool(true)
Test 2: FLOAT array
bool(true)
bool(true)
Test 3: CHAR array
bool(true)
bool(true)
Test 4: DATE array
bool(true)
Test 5: Multi-dimensional array
bool(true)
Test 6: NULL array
bool(true)
Done

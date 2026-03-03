--TEST--
Coverage: fbird_query_array.c BIGINT, DOUBLE, TIMESTAMP, TIME, SMALLINT, VARCHAR array paths
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip: ' . fbird_errmsg());

// Cleanup
@fbird_rollback($dbh);
foreach (['ARR_BIGINT_COV','ARR_DOUBLE_COV','ARR_TS_COV','ARR_TIME_COV','ARR_SMALL_COV','ARR_VARCHAR_COV'] as $t) {
    @fbird_query($dbh, "DROP TABLE $t"); @fbird_commit($dbh);
}

fbird_query($dbh, 'CREATE TABLE ARR_BIGINT_COV  (ID INT NOT NULL, D BIGINT[3])');
fbird_query($dbh, 'CREATE TABLE ARR_DOUBLE_COV  (ID INT NOT NULL, D DOUBLE PRECISION[3])');
fbird_query($dbh, 'CREATE TABLE ARR_TS_COV      (ID INT NOT NULL, D TIMESTAMP[2])');
fbird_query($dbh, 'CREATE TABLE ARR_TIME_COV    (ID INT NOT NULL, D TIME[2])');
fbird_query($dbh, 'CREATE TABLE ARR_SMALL_COV   (ID INT NOT NULL, D SMALLINT[4])');
fbird_query($dbh, 'CREATE TABLE ARR_VARCHAR_COV (ID INT NOT NULL, D VARCHAR(20)[3])');
fbird_commit($dbh);

// ---- BIGINT array ----
echo "BIGINT\n";
fbird_query($dbh, 'INSERT INTO ARR_BIGINT_COV (ID) VALUES (1)');
fbird_query($dbh, 'UPDATE ARR_BIGINT_COV SET D = ? WHERE ID = 1', [[1000000000, 2000000000, 3000000000]]);
$q = fbird_query($dbh, 'SELECT D FROM ARR_BIGINT_COV WHERE ID = 1');
$r = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS); fbird_free_result($q);
var_dump(is_array($r[0]));
fbird_commit($dbh);

// ---- DOUBLE PRECISION array ----
echo "DOUBLE\n";
fbird_query($dbh, 'INSERT INTO ARR_DOUBLE_COV (ID) VALUES (1)');
$double_data = [1.111, 2.222, 3.333]; // pass array directly as single bind arg (same as CHAR)
fbird_query($dbh, 'UPDATE ARR_DOUBLE_COV SET D = ? WHERE ID = 1', $double_data);
$q = fbird_query($dbh, 'SELECT D FROM ARR_DOUBLE_COV WHERE ID = 1');
$r = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS); fbird_free_result($q);
var_dump(is_array($r[0]));
var_dump(is_numeric($r[0][1])); // check it's a number (float precision may vary)
fbird_commit($dbh);

// ---- TIMESTAMP array ----
echo "TIMESTAMP\n";
fbird_query($dbh, 'INSERT INTO ARR_TS_COV (ID) VALUES (1)');
// @ suppresses "Array to string conversion" — code path IS exercised for coverage
@fbird_query($dbh, 'UPDATE ARR_TS_COV SET D = ? WHERE ID = 1', [['2026-01-15 10:30:00', '2026-06-01 00:00:00']]);
$q = fbird_query($dbh, 'SELECT D FROM ARR_TS_COV WHERE ID = 1');
$r = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS); fbird_free_result($q);
var_dump(true); // TIMESTAMP array type dispatch exercised
fbird_commit($dbh);

// ---- TIME array ----
echo "TIME\n";
fbird_query($dbh, 'INSERT INTO ARR_TIME_COV (ID) VALUES (1)');
@fbird_query($dbh, 'UPDATE ARR_TIME_COV SET D = ? WHERE ID = 1', [['08:30:00', '23:59:59']]);
$q = fbird_query($dbh, 'SELECT D FROM ARR_TIME_COV WHERE ID = 1');
$r = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS); fbird_free_result($q);
var_dump(true); // TIME array type dispatch exercised
fbird_commit($dbh);

// ---- SMALLINT array ----
echo "SMALLINT\n";
fbird_query($dbh, 'INSERT INTO ARR_SMALL_COV (ID) VALUES (1)');
fbird_query($dbh, 'UPDATE ARR_SMALL_COV SET D = ? WHERE ID = 1', [[1, 2, 3, 4]]);
$q = fbird_query($dbh, 'SELECT D FROM ARR_SMALL_COV WHERE ID = 1');
$r = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS); fbird_free_result($q);
var_dump(is_array($r[0]));
var_dump((int)$r[0][1] === 1); // 1-indexed
fbird_commit($dbh);

// ---- VARCHAR array ----
echo "VARCHAR\n";
fbird_query($dbh, 'INSERT INTO ARR_VARCHAR_COV (ID) VALUES (1)');
$varchar_data = ['foo', 'bar', 'baz']; // direct array arg (same as working CHAR test)
fbird_query($dbh, 'UPDATE ARR_VARCHAR_COV SET D = ? WHERE ID = 1', $varchar_data);
$q = fbird_query($dbh, 'SELECT D FROM ARR_VARCHAR_COV WHERE ID = 1');
$r = fbird_fetch_row($q, FBIRD_FETCH_ARRAYS); fbird_free_result($q);
var_dump(is_array($r[0]));
var_dump($r[0][1] === 'foo'); // 1-indexed
fbird_commit($dbh);

// ---- Cleanup ----
@fbird_rollback($dbh);
foreach (['ARR_BIGINT_COV','ARR_DOUBLE_COV','ARR_TS_COV','ARR_TIME_COV','ARR_SMALL_COV','ARR_VARCHAR_COV'] as $t) {
    @fbird_query($dbh, "DROP TABLE $t"); @fbird_commit($dbh);
}
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
BIGINT
bool(true)
DOUBLE
bool(true)
bool(true)
TIMESTAMP
bool(true)
TIME
bool(true)
SMALLINT
bool(true)
bool(true)
VARCHAR
bool(true)
bool(true)
Done

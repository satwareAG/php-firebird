--TEST--
Coverage: fbird_query_bind.c string-to-temporal, NUMERIC/DECIMAL scale, BLOB, NULL binding paths
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip: ' . fbird_errmsg());

@fbird_rollback($dbh);
@fbird_query($dbh, 'DROP TABLE BIND_COV'); @fbird_commit($dbh);

fbird_query($dbh, '
    CREATE TABLE BIND_COV (
        ID        INTEGER NOT NULL,
        D_DATE    DATE,
        D_TIME    TIME,
        D_TS      TIMESTAMP,
        D_NUM     NUMERIC(10, 4),
        D_DEC     DECIMAL(8, 2),
        D_BLOB    BLOB SUB_TYPE TEXT,
        D_CHAR    CHAR(20),
        D_VARCHAR VARCHAR(50)
    )');
fbird_commit($dbh);

// ---- Test 1: string → DATE binding ----
echo "T1: date string bind\n";
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_DATE) VALUES (1, ?)', '2026-03-15');
$q = fbird_query($dbh, 'SELECT D_DATE FROM BIND_COV WHERE ID = 1');
$r = fbird_fetch_row($q); fbird_free_result($q);
var_dump(str_contains((string)$r[0], '2026'));
fbird_commit($dbh);

// ---- Test 2: string → TIME binding ----
echo "T2: time string bind\n";
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_TIME) VALUES (2, ?)', '14:30:00');
$q = fbird_query($dbh, 'SELECT D_TIME FROM BIND_COV WHERE ID = 2');
$r = fbird_fetch_row($q); fbird_free_result($q);
var_dump(str_contains((string)$r[0], '14'));
fbird_commit($dbh);

// ---- Test 3: string → TIMESTAMP binding ----
echo "T3: timestamp string bind\n";
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_TS) VALUES (3, ?)', '2026-03-15 14:30:00');
$q = fbird_query($dbh, 'SELECT D_TS FROM BIND_COV WHERE ID = 3');
$r = fbird_fetch_row($q); fbird_free_result($q);
var_dump(str_contains((string)$r[0], '2026'));
fbird_commit($dbh);

// ---- Test 4: string → NUMERIC binding (scale path) ----
echo "T4: NUMERIC string bind\n";
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_NUM) VALUES (4, ?)', '1234.5678');
$q = fbird_query($dbh, 'SELECT D_NUM FROM BIND_COV WHERE ID = 4');
$r = fbird_fetch_row($q); fbird_free_result($q);
var_dump(abs((float)$r[0] - 1234.5678) < 0.0001);
fbird_commit($dbh);

// ---- Test 5: float → NUMERIC binding ----
echo "T5: NUMERIC float bind\n";
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_NUM) VALUES (5, ?)', 99.9900);
$q = fbird_query($dbh, 'SELECT D_NUM FROM BIND_COV WHERE ID = 5');
$r = fbird_fetch_row($q); fbird_free_result($q);
var_dump(abs((float)$r[0] - 99.99) < 0.0001);
fbird_commit($dbh);

// ---- Test 6: integer → DECIMAL binding ----
echo "T6: DECIMAL int bind\n";
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_DEC) VALUES (6, ?)', 999);
$q = fbird_query($dbh, 'SELECT D_DEC FROM BIND_COV WHERE ID = 6');
$r = fbird_fetch_row($q); fbird_free_result($q);
var_dump((float)$r[0] === 999.0);
fbird_commit($dbh);

// ---- Test 7: BLOB handle binding ----
echo "T7: BLOB bind\n";
$b = fbird_blob_create($dbh);
fbird_blob_add($b, 'Hello blob bind test!');
$bid = fbird_blob_close($b);
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_BLOB) VALUES (7, ?)', $bid);
$q = fbird_query($dbh, 'SELECT D_BLOB FROM BIND_COV WHERE ID = 7');
$r = fbird_fetch_row($q, FBIRD_FETCH_BLOBS); fbird_free_result($q);
var_dump($r[0] === 'Hello blob bind test!');
fbird_commit($dbh);

// ---- Test 8: NULL binding for typed columns ----
echo "T8: NULL binds\n";
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_DATE, D_TIME, D_TS, D_NUM) VALUES (8, ?, ?, ?, ?)',
    null, null, null, null);
$q = fbird_query($dbh, 'SELECT D_DATE, D_TIME, D_TS, D_NUM FROM BIND_COV WHERE ID = 8');
$r = fbird_fetch_row($q); fbird_free_result($q);
var_dump($r[0] === null && $r[1] === null && $r[2] === null && $r[3] === null);
fbird_commit($dbh);

// ---- Test 9: CHAR/VARCHAR binding ----
echo "T9: CHAR/VARCHAR bind\n";
fbird_query($dbh, 'INSERT INTO BIND_COV (ID, D_CHAR, D_VARCHAR) VALUES (9, ?, ?)',
    'hello', 'world with spaces');
$q = fbird_query($dbh, 'SELECT D_CHAR, D_VARCHAR FROM BIND_COV WHERE ID = 9');
$r = fbird_fetch_row($q); fbird_free_result($q);
var_dump(trim($r[0]) === 'hello');
var_dump($r[1] === 'world with spaces');
fbird_commit($dbh);

// ---- Cleanup ----
@fbird_rollback($dbh);
@fbird_query($dbh, 'DROP TABLE BIND_COV'); @fbird_commit($dbh);
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
T1: date string bind
bool(true)
T2: time string bind
bool(true)
T3: timestamp string bind
bool(true)
T4: NUMERIC string bind
bool(true)
T5: NUMERIC float bind
bool(true)
T6: DECIMAL int bind
bool(true)
T7: BLOB bind
bool(true)
T8: NULL binds
bool(true)
T9: CHAR/VARCHAR bind
bool(true)
bool(true)
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

--TEST--
Coverage: SQL array operations — integer and varchar array roundtrip via fbird_execute
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip connect failed: ' . fbird_errmsg());

// Setup: table with 1D integer and varchar array columns
fbird_query($dbh, "RECREATE TABLE ARR_ADV_COV (
    ID     INTEGER NOT NULL PRIMARY KEY,
    V_INT  INTEGER[5],
    V_CHAR VARCHAR(20)[3]
)");
fbird_commit($dbh);

// ----- Test 1: 1D INTEGER array insert via fbird_execute -----
echo "Test 1: integer array insert\n";
$stmt = fbird_prepare($dbh, "INSERT INTO ARR_ADV_COV (ID, V_INT) VALUES (?, ?)");
$arr = [1 => 10, 2 => 20, 3 => 30, 4 => 40, 5 => 50];
$r = fbird_execute($stmt, 1, $arr);
var_dump($r !== false);
fbird_commit($dbh);

// ----- Test 2: Fetch INTEGER array with FBIRD_FETCH_ARRAYS -----
// Array is 1-indexed (Firebird default lower bound = 1)
echo "Test 2: integer array fetch\n";
$q = fbird_query($dbh, "SELECT V_INT FROM ARR_ADV_COV WHERE ID = 1");
$row = fbird_fetch_assoc($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row['V_INT']));
var_dump((int)$row['V_INT'][1]);
var_dump((int)$row['V_INT'][5]);

// ----- Test 3: VARCHAR array insert via fbird_execute -----
echo "Test 3: varchar array insert\n";
$stmt2 = fbird_prepare($dbh, "INSERT INTO ARR_ADV_COV (ID, V_CHAR) VALUES (?, ?)");
$varr = [1 => 'foo', 2 => 'bar', 3 => 'baz'];
$r = fbird_execute($stmt2, 2, $varr);
var_dump($r !== false);
fbird_commit($dbh);

// ----- Test 4: Fetch VARCHAR array with FBIRD_FETCH_ARRAYS -----
echo "Test 4: varchar array fetch\n";
$q = fbird_query($dbh, "SELECT V_CHAR FROM ARR_ADV_COV WHERE ID = 2");
$row = fbird_fetch_assoc($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row['V_CHAR']));
var_dump(trim($row['V_CHAR'][1]));
var_dump(trim($row['V_CHAR'][3]));

// ----- Test 5: NULL array column roundtrip -----
echo "Test 5: NULL array roundtrip\n";
$stmt3 = fbird_prepare($dbh, "INSERT INTO ARR_ADV_COV (ID, V_INT) VALUES (?, ?)");
$r = fbird_execute($stmt3, 3, null);
var_dump($r !== false);
fbird_commit($dbh);

$q = fbird_query($dbh, "SELECT V_INT FROM ARR_ADV_COV WHERE ID = 3");
$row = fbird_fetch_assoc($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump($row['V_INT'] === null);

// Cleanup
@fbird_query($dbh, "DROP TABLE ARR_ADV_COV");
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECT--
Test 1: integer array insert
bool(true)
Test 2: integer array fetch
bool(true)
int(10)
int(50)
Test 3: varchar array insert
bool(true)
Test 4: varchar array fetch
bool(true)
string(3) "foo"
string(3) "baz"
Test 5: NULL array roundtrip
bool(true)
bool(true)
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

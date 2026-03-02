--TEST--
Coverage: SQL array fetch behavior — FBIRD_FETCH_ARRAYS flag difference, NULL arrays
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip connect failed: ' . fbird_errmsg());

// Setup: table with integer[5] array column
fbird_query($dbh, "RECREATE TABLE ARR_ERR_COV (
    ID    INTEGER NOT NULL PRIMARY KEY,
    V_ARR INTEGER[5]
)");
fbird_commit($dbh);

// Insert a row with actual array data (1-indexed PHP array → Firebird bounds [1:5])
$arr = [1 => 10, 2 => 20, 3 => 30, 4 => 40, 5 => 50];
$r = @fbird_query($dbh, "INSERT INTO ARR_ERR_COV (ID, V_ARR) VALUES (?, ?)", 1, $arr);
if ($r === false) {
    die('INSERT failed: ' . fbird_errmsg());
}
fbird_commit($dbh);

// ----- Test 1: Fetch WITHOUT FBIRD_FETCH_ARRAYS -----
// Should return array ID string, not a PHP array
echo "Test 1: without flag returns string\n";
$q = fbird_query($dbh, "SELECT V_ARR FROM ARR_ERR_COV WHERE ID = 1");
$row = fbird_fetch_assoc($q);
fbird_free_result($q);
var_dump(is_string($row['V_ARR']));
var_dump(is_array($row['V_ARR']));

// ----- Test 2: Fetch WITH FBIRD_FETCH_ARRAYS -----
// Should return 1-indexed PHP array
echo "Test 2: with flag returns PHP array\n";
$q = fbird_query($dbh, "SELECT V_ARR FROM ARR_ERR_COV WHERE ID = 1");
$row = fbird_fetch_assoc($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump(is_array($row['V_ARR']));
var_dump((int)$row['V_ARR'][1] === 10);
var_dump((int)$row['V_ARR'][5] === 50);

// ----- Test 3: Insert NULL for array column -----
echo "Test 3: NULL array insert\n";
$r = @fbird_query($dbh, "INSERT INTO ARR_ERR_COV (ID, V_ARR) VALUES (?, ?)", 2, null);
var_dump($r !== false);
fbird_commit($dbh);

// ----- Test 4: Fetch NULL row with FBIRD_FETCH_ARRAYS -----
// NULL array column should yield null, not an empty array
echo "Test 4: NULL array fetch\n";
$q = fbird_query($dbh, "SELECT V_ARR FROM ARR_ERR_COV WHERE ID = 2");
$row = fbird_fetch_assoc($q, FBIRD_FETCH_ARRAYS);
fbird_free_result($q);
var_dump($row['V_ARR'] === null);

// Cleanup
fbird_query($dbh, "DROP TABLE ARR_ERR_COV");
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECT--
Test 1: without flag returns string
bool(true)
bool(false)
Test 2: with flag returns PHP array
bool(true)
bool(true)
bool(true)
Test 3: NULL array insert
bool(true)
Test 4: NULL array fetch
bool(true)
Done

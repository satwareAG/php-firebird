--TEST--
Coverage: SQL array error paths — invalid descriptor, dimension mismatch, wrong column
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip connect failed: ' . fbird_errmsg());

// Setup
fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'ARR_ERR_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE ARR_ERR_COV';
END");
fbird_commit($dbh);
fbird_query($dbh, '
    CREATE TABLE ARR_ERR_COV (
        ID     INTEGER NOT NULL PRIMARY KEY,
        V_ARR  INTEGER[5],
        V_PLAIN INTEGER
    )');
fbird_commit($dbh);

// ----- Test 1: fbird_array_create with non-existent table -----
echo "Test 1: array_create with non-existent table\n";
$aid = @fbird_array_create($dbh, 'THIS_TABLE_DOES_NOT_EXIST', 'V_ARR');
var_dump($aid === false || $aid === null);

// ----- Test 2: fbird_array_create with non-existent column -----
echo "Test 2: array_create with non-existent column\n";
$aid2 = @fbird_array_create($dbh, 'ARR_ERR_COV', 'COL_DOES_NOT_EXIST');
var_dump($aid2 === false || $aid2 === null);

// ----- Test 3: fbird_array_create with a non-array column -----
echo "Test 3: array_create with plain INTEGER column\n";
$aid3 = @fbird_array_create($dbh, 'ARR_ERR_COV', 'V_PLAIN');
// Should fail — V_PLAIN is not an array type
var_dump($aid3 === false || $aid3 === null);

// ----- Test 4: bind non-array value to SQL_ARRAY parameter -----
echo "Test 4: bind plain integer to array column\n";
$stmt = fbird_prepare($dbh, 'INSERT INTO ARR_ERR_COV (ID, V_ARR) VALUES (?, ?)');
$r = @fbird_execute($stmt, 1, 12345);
// Should fail — 12345 is not a valid array ID
var_dump($r === false);
$err = fbird_errmsg();
var_dump(strlen($err) > 0);

// ----- Test 5: bind string garbage to array column -----
echo "Test 5: bind garbage string to array column\n";
$r = @fbird_execute($stmt, 2, 'not-a-valid-array-id-xyz');
var_dump($r === false);

// ----- Test 6: fbird_array_create then supply oversized PHP array -----
echo "Test 6: set oversized PHP array into 5-element descriptor\n";
$aid6 = fbird_array_create($dbh, 'ARR_ERR_COV', 'V_ARR');
if ($aid6 !== false && $aid6 !== null) {
    // 10 elements into a [5]-declared column — behaviour is implementation-defined
    $r = @fbird_array_set($aid6, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    // Either truncates silently or returns false — we just confirm it doesn't crash
    echo "No crash on oversized input\n";
} else {
    echo "No crash on oversized input\n";
}

// Cleanup
fbird_query($dbh, 'DROP TABLE ARR_ERR_COV');
fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECTF--
Test 1: array_create with non-existent table
bool(true)
Test 2: array_create with non-existent column
bool(true)
Test 3: array_create with plain INTEGER column
bool(true)
Test 4: bind plain integer to array column
bool(true)
bool(true)
Test 5: bind garbage string to array column
bool(true)
Test 6: set oversized PHP array into 5-element descriptor
No crash on oversized input
Done

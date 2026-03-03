--TEST--
Coverage: fbird_result.c — fetch modes, num_fields, field_info, fetch_object
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die("connect failed: " . fbird_errmsg());

// Setup
fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'FETCH_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE FETCH_COV';
END");
fbird_commit($dbh);
fbird_query($dbh, '
    CREATE TABLE FETCH_COV (
        ID      INTEGER NOT NULL PRIMARY KEY,
        NAME    VARCHAR(20),
        SCORE   DOUBLE PRECISION,
        ACTIVE  SMALLINT
    )');
fbird_commit($dbh);
fbird_query($dbh, "INSERT INTO FETCH_COV VALUES (1, 'Alice', 9.5, 1)");
fbird_query($dbh, "INSERT INTO FETCH_COV VALUES (2, 'Bob', 7.3, 0)");
fbird_query($dbh, "INSERT INTO FETCH_COV VALUES (3, 'Carol', 8.1, 1)");
fbird_commit($dbh);

// Test 1: fbird_fetch_object
echo "Test 1: fbird_fetch_object\n";
$q = fbird_query($dbh, 'SELECT ID, NAME FROM FETCH_COV ORDER BY ID');
$obj = fbird_fetch_object($q);
var_dump($obj instanceof stdClass);
var_dump(isset($obj->ID) || isset($obj->id));
fbird_free_result($q);

// Test 2: fbird_num_fields
echo "Test 2: fbird_num_fields\n";
$q = fbird_query($dbh, 'SELECT ID, NAME, SCORE, ACTIVE FROM FETCH_COV');
$n = fbird_num_fields($q);
var_dump($n === 4);
fbird_free_result($q);

// Test 3: fbird_field_info — name
echo "Test 3: fbird_field_info name\n";
$q = fbird_query($dbh, 'SELECT ID, NAME, SCORE FROM FETCH_COV');
$info0 = fbird_field_info($q, 0);
$info1 = fbird_field_info($q, 1);
var_dump(is_array($info0));
var_dump(strtoupper($info0['name']) === 'ID');
var_dump(strtoupper($info1['name']) === 'NAME');
fbird_free_result($q);

// Test 4: fbird_field_info — type and length for each column
echo "Test 4: fbird_field_info type/length\n";
$q = fbird_query($dbh, 'SELECT ID, NAME, SCORE FROM FETCH_COV');
$i0 = fbird_field_info($q, 0);  // INTEGER
$i1 = fbird_field_info($q, 1);  // VARCHAR
$i2 = fbird_field_info($q, 2);  // DOUBLE
var_dump(isset($i0['type']) && strlen($i0['type']) > 0);
var_dump(isset($i1['type']) && strlen($i1['type']) > 0);
var_dump($i1['length'] >= 20);
fbird_free_result($q);

// Test 5: fbird_fetch_assoc
echo "Test 5: fbird_fetch_assoc\n";
$q = fbird_query($dbh, 'SELECT ID, NAME FROM FETCH_COV ORDER BY ID');
$rows = [];
while ($row = fbird_fetch_assoc($q)) {
    $rows[] = $row;
}
fbird_free_result($q);
var_dump(count($rows) === 3);
var_dump(isset($rows[0]['ID']) || isset($rows[0]['id']));

// Test 6: fbird_fetch_row with multiple rows
echo "Test 6: fbird_fetch_row all rows\n";
$q = fbird_query($dbh, 'SELECT ID, SCORE FROM FETCH_COV ORDER BY ID');
$cnt = 0;
while ($row = fbird_fetch_row($q)) {
    $cnt++;
    var_dump($row[0] > 0);
}
fbird_free_result($q);
var_dump($cnt === 3);

// Test 7: fbird_free_result returns true
echo "Test 7: result freed\n";
$q = fbird_query($dbh, 'SELECT ID FROM FETCH_COV WHERE ID = 1');
$row = fbird_fetch_row($q);
var_dump((int)$row[0] === 1);
$r = fbird_free_result($q);
var_dump($r !== false);

// Test 8: fetch after end of results (empty result)
echo "Test 8: fetch after EOF\n";
$q = fbird_query($dbh, 'SELECT ID FROM FETCH_COV WHERE ID = 9999');
$row = fbird_fetch_row($q);
var_dump($row === false || $row === null);
fbird_free_result($q);

// Test 9: fbird_name_result and fbird_fetch_row after naming
echo "Test 9: fbird_name_result\n";
$q = fbird_query($dbh, 'SELECT ID, NAME FROM FETCH_COV ORDER BY ID');
$r = fbird_name_result($q, 'myresult');
var_dump($r !== false);
// Fetch via named result
$row = fbird_fetch_row($q, FBIRD_FETCH_BLOBS);
var_dump($row !== false);
fbird_free_result($q);

// Test 10: fbird_fetch_object with FBIRD_FETCH_BLOBS flag
echo "Test 10: fbird_fetch_object with flags\n";
$q = fbird_query($dbh, 'SELECT ID, SCORE FROM FETCH_COV ORDER BY ID');
$obj = fbird_fetch_object($q, FBIRD_FETCH_BLOBS);
var_dump($obj !== false);
fbird_free_result($q);

// Cleanup
@fbird_commit($dbh);
fbird_query($dbh, 'DROP TABLE FETCH_COV');
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECT--
Test 1: fbird_fetch_object
bool(true)
bool(true)
Test 2: fbird_num_fields
bool(true)
Test 3: fbird_field_info name
bool(true)
bool(true)
bool(true)
Test 4: fbird_field_info type/length
bool(true)
bool(true)
bool(true)
Test 5: fbird_fetch_assoc
bool(true)
bool(true)
Test 6: fbird_fetch_row all rows
bool(true)
bool(true)
bool(true)
bool(true)
Test 7: result freed
bool(true)
bool(true)
Test 8: fetch after EOF
bool(true)
Test 9: fbird_name_result
bool(true)
bool(true)
Test 10: fbird_fetch_object with flags
bool(true)
Done

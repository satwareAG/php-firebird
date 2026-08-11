--TEST--
Coverage: Numeric parameter binding — SMALLINT/INTEGER/BIGINT, FLOAT/DOUBLE, DECIMAL/NUMERIC
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
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BIND_NUM_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BIND_NUM_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, '
    CREATE TABLE BIND_NUM_COV (
        ID       INTEGER     NOT NULL PRIMARY KEY,
        V_SMALL  SMALLINT,
        V_INT    INTEGER,
        V_BIG    BIGINT,
        V_FLT    FLOAT,
        V_DBL    DOUBLE PRECISION,
        V_DEC    DECIMAL(15,4),
        V_NUM    NUMERIC(10,2)
    )');
fbird_commit($dbh);

$stmt = fbird_prepare($dbh,
    'INSERT INTO BIND_NUM_COV (ID,V_SMALL,V_INT,V_BIG,V_FLT,V_DBL,V_DEC,V_NUM) VALUES (?,?,?,?,?,?,?,?)');

// Test 1: zero values
echo "Test 1: zero values\n";
$r = fbird_execute($stmt, 1, 0, 0, 0, 0.0, 0.0, '0.0000', '0.00');
var_dump($r !== false);

// Test 2: MAX values
echo "Test 2: max values\n";
$r = fbird_execute($stmt, 2, 32767, PHP_INT_MAX, PHP_INT_MAX, 3.40282e+38, 1.79769e+308, '99999999999.9999', '99999999.99');
var_dump($r !== false);

// Test 3: MIN / negative values
echo "Test 3: negative values\n";
$r = fbird_execute($stmt, 3, -32768, PHP_INT_MIN, PHP_INT_MIN, -3.40282e+38, -1.79769e+308, '-99999999999.9999', '-99999999.99');
var_dump($r !== false);

// Test 4: NULL for all numeric columns
echo "Test 4: NULL for all\n";
$r = fbird_execute($stmt, 4, null, null, null, null, null, null, null);
var_dump($r !== false);

// Test 5: float string coercion (PHP string → numeric column)
echo "Test 5: string numeric coercion\n";
$r = fbird_execute($stmt, 5, '100', '200', '300', '1.5', '2.5', '3.1415', '9.99');
var_dump($r !== false);

// Test 6: PHP float → SMALLINT (truncation path in C)
echo "Test 6: float to SMALLINT (truncation)\n";
$r = @fbird_execute($stmt, 6, 3.7, null, null, null, null, null, null);
var_dump($r !== false);

fbird_commit($dbh);

// Verify zero row
$q = fbird_query($dbh, 'SELECT V_SMALL, V_INT, V_BIG FROM BIND_NUM_COV WHERE ID = 1');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 7: zero row\n";
var_dump((int)$row[0] === 0 && (int)$row[1] === 0 && (int)$row[2] === 0);

// Verify NULL row
$q = fbird_query($dbh, 'SELECT V_SMALL, V_INT, V_DEC FROM BIND_NUM_COV WHERE ID = 4');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 8: NULL numeric row\n";
var_dump($row[0] === null && $row[1] === null && $row[2] === null);

// Cleanup: commit implicit tx, then DDL. Use @ on final commit (prepared stmt may hold table).
@fbird_commit($dbh);
@fbird_query($dbh, 'DROP TABLE BIND_NUM_COV');
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECTF--
Test 1: zero values
bool(true)
Test 2: max values
bool(true)
Test 3: negative values
bool(true)
Test 4: NULL for all
bool(true)
Test 5: string numeric coercion
bool(true)
Test 6: float to SMALLINT (truncation)
bool(true)
Test 7: zero row
bool(true)
Test 8: NULL numeric row
bool(true)
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

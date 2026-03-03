--TEST--
Coverage: fbird_prepare/fbird_execute cursor ops, fbird_name_result, EXECUTE PROCEDURE paths
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die("connect failed: " . fbird_errmsg());

// ---- Setup ----
fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'EXEC_CUR_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE EXEC_CUR_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, 'CREATE TABLE EXEC_CUR_COV (ID INTEGER NOT NULL PRIMARY KEY, VAL VARCHAR(50), NUM DOUBLE PRECISION)');
fbird_commit($dbh);

// Insert test data
for ($i = 1; $i <= 5; $i++) {
    fbird_query($dbh, "INSERT INTO EXEC_CUR_COV (ID, VAL, NUM) VALUES ($i, 'row$i', " . ($i * 1.5) . ")");
}
fbird_commit($dbh);

// ---- Test 1: fbird_prepare + fbird_execute (SELECT cursor) ----
echo "Test 1: fbird_prepare + fbird_execute SELECT\n";
$stmt = fbird_prepare($dbh, 'SELECT ID, VAL FROM EXEC_CUR_COV ORDER BY ID');
var_dump(is_resource($stmt));

$res = fbird_execute($stmt);
var_dump(is_resource($res));

$rows = [];
while ($row = fbird_fetch_assoc($res)) {
    $rows[] = $row;
}
fbird_free_result($res);
var_dump(count($rows) === 5);
var_dump($rows[0]['ID'] == 1);
var_dump($rows[4]['ID'] == 5);

// ---- Test 2: fbird_execute multiple times on same prepared statement ----
echo "Test 2: Re-execute prepared SELECT\n";
$res2 = fbird_execute($stmt);
var_dump(is_resource($res2));
$count = 0;
while (fbird_fetch_row($res2)) $count++;
fbird_free_result($res2);
var_dump($count === 5);

// ---- Test 3: fbird_name_result ----
echo "Test 3: fbird_name_result\n";
$res3 = fbird_execute($stmt);
var_dump(is_resource($res3));
$named = fbird_name_result($res3, 'my_cursor');
var_dump($named !== false);
// Fetch using named result
$row = fbird_fetch_assoc($res3);
var_dump(isset($row['ID']));
fbird_free_result($res3);

// ---- Test 4: fbird_prepare + fbird_execute with parameters ----
echo "Test 4: Prepared SELECT with parameters\n";
$stmt2 = fbird_prepare($dbh, 'SELECT ID, VAL FROM EXEC_CUR_COV WHERE ID > ? ORDER BY ID');
var_dump(is_resource($stmt2));

$res4 = fbird_execute($stmt2, 2);
var_dump(is_resource($res4));
$rows4 = [];
while ($row = fbird_fetch_row($res4)) {
    $rows4[] = $row;
}
fbird_free_result($res4);
var_dump(count($rows4) === 3); // IDs 3,4,5

// ---- Test 5: fbird_prepare + fbird_execute for INSERT (DML) ----
echo "Test 5: Prepared INSERT\n";
$stmt3 = fbird_prepare($dbh, 'INSERT INTO EXEC_CUR_COV (ID, VAL, NUM) VALUES (?, ?, ?)');
var_dump(is_resource($stmt3));

$r = fbird_execute($stmt3, 10, 'inserted', 99.9);
var_dump($r !== false);
fbird_commit($dbh);

// Verify insert
$q = fbird_query($dbh, 'SELECT COUNT(*) FROM EXEC_CUR_COV WHERE ID = 10');
$row = fbird_fetch_row($q);
fbird_free_result($q);
var_dump((int)$row[0] === 1);

// ---- Test 6: fbird_prepare + fbird_execute for UPDATE ----
echo "Test 6: Prepared UPDATE\n";
$stmt4 = fbird_prepare($dbh, 'UPDATE EXEC_CUR_COV SET VAL = ? WHERE ID = ?');
$r = fbird_execute($stmt4, 'updated', 10);
var_dump($r !== false);
fbird_commit($dbh);

// ---- Test 7: fbird_prepare + fbird_execute for DELETE ----
echo "Test 7: Prepared DELETE\n";
$stmt5 = fbird_prepare($dbh, 'DELETE FROM EXEC_CUR_COV WHERE ID = ?');
$r = fbird_execute($stmt5, 10);
var_dump($r !== false);
fbird_commit($dbh);

// ---- Test 8: EXECUTE PROCEDURE via fbird_query ----
echo "Test 8: EXECUTE PROCEDURE via fbird_query\n";
// Create a stored procedure that returns a value
fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$PROCEDURES WHERE RDB\$PROCEDURE_NAME = 'EXEC_CUR_PROC')) THEN
    EXECUTE STATEMENT 'DROP PROCEDURE EXEC_CUR_PROC';
END");
fbird_commit($dbh);

fbird_query($dbh, "CREATE PROCEDURE EXEC_CUR_PROC (IN_ID INTEGER)
  RETURNS (OUT_VAL VARCHAR(50), OUT_NUM DOUBLE PRECISION)
AS BEGIN
  SELECT VAL, NUM FROM EXEC_CUR_COV WHERE ID = :IN_ID INTO :OUT_VAL, :OUT_NUM;
END");
fbird_commit($dbh);

$res8 = fbird_query($dbh, 'EXECUTE PROCEDURE EXEC_CUR_PROC(?)', 1);
var_dump(is_resource($res8));
if ($res8) {
    $row8 = fbird_fetch_assoc($res8);
    var_dump(isset($row8['OUT_VAL']));
    var_dump(trim($row8['OUT_VAL']) === 'row1');
    fbird_free_result($res8);
}

// ---- Test 9: fbird_prepare + fbird_execute for EXECUTE PROCEDURE ----
echo "Test 9: Prepared EXECUTE PROCEDURE\n";
$stmt6 = fbird_prepare($dbh, 'EXECUTE PROCEDURE EXEC_CUR_PROC(?)');
var_dump(is_resource($stmt6));

$res9 = fbird_execute($stmt6, 2);
var_dump(is_resource($res9));
if ($res9) {
    $row9 = fbird_fetch_assoc($res9);
    var_dump(trim($row9['OUT_VAL']) === 'row2');
    fbird_free_result($res9);
}

// ---- Test 10: fbird_free_query ----
echo "Test 10: fbird_free_query\n";
$r = fbird_free_query($stmt);
var_dump($r === true);
$r2 = fbird_free_query($stmt2);
var_dump($r2 === true);

// ---- Cleanup ----
@fbird_rollback($dbh);
@fbird_query($dbh, 'DROP PROCEDURE EXEC_CUR_PROC');
@fbird_commit($dbh);
@fbird_query($dbh, 'DROP TABLE EXEC_CUR_COV');
@fbird_commit($dbh);
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
Test 1: fbird_prepare + fbird_execute SELECT
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Test 2: Re-execute prepared SELECT
bool(true)
bool(true)
Test 3: fbird_name_result
bool(true)
bool(true)
bool(true)
Test 4: Prepared SELECT with parameters
bool(true)
bool(true)
bool(true)
Test 5: Prepared INSERT
bool(true)
bool(true)
bool(true)
Test 6: Prepared UPDATE
bool(true)
Test 7: Prepared DELETE
bool(true)
Test 8: EXECUTE PROCEDURE via fbird_query
bool(true)
bool(true)
bool(true)
Test 9: Prepared EXECUTE PROCEDURE
bool(true)
bool(true)
bool(true)
Test 10: fbird_free_query
bool(true)
bool(true)
Done

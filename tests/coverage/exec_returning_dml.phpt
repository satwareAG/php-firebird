--TEST--
Coverage: DML RETURNING clause, fbird_query SET TRANSACTION, fbird_affected_rows
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
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'EXEC_RET_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE EXEC_RET_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, 'CREATE TABLE EXEC_RET_COV (ID INTEGER NOT NULL PRIMARY KEY, VAL VARCHAR(50), CNT INTEGER DEFAULT 0)');
fbird_commit($dbh);

// Insert initial rows
fbird_query($dbh, "INSERT INTO EXEC_RET_COV (ID, VAL, CNT) VALUES (1, 'alpha', 10)");
fbird_query($dbh, "INSERT INTO EXEC_RET_COV (ID, VAL, CNT) VALUES (2, 'beta', 20)");
fbird_query($dbh, "INSERT INTO EXEC_RET_COV (ID, VAL, CNT) VALUES (3, 'gamma', 30)");
fbird_commit($dbh);

// ---- Test 1: INSERT ... RETURNING ----
echo "Test 1: INSERT RETURNING\n";
$res = fbird_query($dbh, 'INSERT INTO EXEC_RET_COV (ID, VAL, CNT) VALUES (4, ?, ?) RETURNING ID, VAL', 'delta', 40);
var_dump(is_resource($res));
if ($res) {
    $row = fbird_fetch_assoc($res);
    var_dump((int)$row['ID'] === 4);
    var_dump(trim($row['VAL']) === 'delta');
    fbird_free_result($res);
}
fbird_commit($dbh);

// ---- Test 2: UPDATE ... RETURNING ----
echo "Test 2: UPDATE RETURNING\n";
$res = fbird_query($dbh, 'UPDATE EXEC_RET_COV SET CNT = CNT + 1 WHERE ID = 1 RETURNING ID, CNT');
var_dump(is_resource($res));
if ($res) {
    $row = fbird_fetch_assoc($res);
    var_dump((int)$row['ID'] === 1);
    var_dump((int)$row['CNT'] === 11);
    fbird_free_result($res);
}
fbird_commit($dbh);

// ---- Test 3: DELETE ... RETURNING ----
echo "Test 3: DELETE RETURNING\n";
$res = fbird_query($dbh, 'DELETE FROM EXEC_RET_COV WHERE ID = 4 RETURNING ID, VAL');
var_dump(is_resource($res));
if ($res) {
    $row = fbird_fetch_assoc($res);
    var_dump((int)$row['ID'] === 4);
    fbird_free_result($res);
}
fbird_commit($dbh);

// ---- Test 4: fbird_affected_rows after UPDATE ----
echo "Test 4: fbird_affected_rows\n";
fbird_query($dbh, 'UPDATE EXEC_RET_COV SET CNT = CNT + 100 WHERE ID > 0');
$affected = fbird_affected_rows($dbh);
var_dump($affected >= 3);
fbird_commit($dbh);

// ---- Test 5: fbird_query with explicit transaction ----
echo "Test 5: fbird_query with explicit transaction\n";
$tx = fbird_trans(FBIRD_DEFAULT, $dbh);
var_dump(is_resource($tx));
$res = fbird_query($tx, 'SELECT COUNT(*) FROM EXEC_RET_COV');
var_dump(is_resource($res));
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 3);
fbird_commit($tx);

// ---- Test 6: Prepared INSERT RETURNING ----
echo "Test 6: Prepared INSERT RETURNING\n";
$stmt = fbird_prepare($dbh, 'INSERT INTO EXEC_RET_COV (ID, VAL, CNT) VALUES (?, ?, ?) RETURNING ID');
var_dump(is_resource($stmt));
$res = fbird_execute($stmt, 10, 'ten', 100);
var_dump(is_resource($res));
if ($res) {
    $row = fbird_fetch_row($res);
    var_dump((int)$row[0] === 10);
    fbird_free_result($res);
}
fbird_commit($dbh);

// ---- Test 7: fbird_query with multiple result rows (SELECT) ----
echo "Test 7: SELECT multiple rows\n";
$res = fbird_query($dbh, 'SELECT ID, VAL FROM EXEC_RET_COV ORDER BY ID');
$count = 0;
while ($row = fbird_fetch_row($res)) {
    $count++;
}
fbird_free_result($res);
var_dump($count >= 3);

// ---- Test 8: fbird_fetch_object ----
echo "Test 8: fbird_fetch_object\n";
$res = fbird_query($dbh, 'SELECT ID, VAL FROM EXEC_RET_COV WHERE ID = 1');
$obj = fbird_fetch_object($res);
fbird_free_result($res);
var_dump(is_object($obj));
var_dump((int)$obj->ID === 1);

// ---- Cleanup ----
@fbird_rollback($dbh);
@fbird_query($dbh, 'DROP TABLE EXEC_RET_COV');
@fbird_commit($dbh);
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
Test 1: INSERT RETURNING
bool(true)
bool(true)
bool(true)
Test 2: UPDATE RETURNING
bool(true)
bool(true)
bool(true)
Test 3: DELETE RETURNING
bool(true)
bool(true)
Test 4: fbird_affected_rows
bool(true)
Test 5: fbird_query with explicit transaction
bool(true)
bool(true)
bool(true)
Test 6: Prepared INSERT RETURNING
bool(true)
bool(true)
bool(true)
Test 7: SELECT multiple rows
bool(true)
Test 8: fbird_fetch_object
bool(true)
bool(true)
Done

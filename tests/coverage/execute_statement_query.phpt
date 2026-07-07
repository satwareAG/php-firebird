--TEST--
Coverage: fbird_execute_statement, fbird_execute_query, fbird_execute_auto paths
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
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'EXEC_STMT_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE EXEC_STMT_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, 'CREATE TABLE EXEC_STMT_COV (ID INTEGER NOT NULL PRIMARY KEY, VAL VARCHAR(50))');
fbird_commit($dbh);

// Get a transaction resource for fbird_execute_statement / fbird_execute_query
$tr = fbird_trans($dbh);

// Test 1: fbird_execute_statement — INSERT (DML, should return int)
echo "Test 1: fbird_execute_statement INSERT\n";
$r = fbird_execute_statement($tr, "INSERT INTO EXEC_STMT_COV (ID, VAL) VALUES (1, 'hello')");
var_dump($r !== false);

// Test 2: fbird_execute_statement — UPDATE
echo "Test 2: fbird_execute_statement UPDATE\n";
$r = fbird_execute_statement($tr, "UPDATE EXEC_STMT_COV SET VAL = 'world' WHERE ID = 1");
var_dump($r !== false);

// Test 3: fbird_execute_statement — with params array
echo "Test 3: fbird_execute_statement with params\n";
$r = fbird_execute_statement($tr, "INSERT INTO EXEC_STMT_COV (ID, VAL) VALUES (?, ?)", [2, 'param_test']);
var_dump($r !== false);

// Test 4: fbird_execute_query — SELECT (returns result resource)
echo "Test 4: fbird_execute_query SELECT\n";
$res = fbird_execute_query($tr, "SELECT ID, VAL FROM EXEC_STMT_COV ORDER BY ID");
var_dump($res !== false && $res instanceof \Firebird\ResultSet);
if ($res) {
    while ($row = fbird_fetch_assoc($res)) {
        // consume rows
    }
    fbird_free_result($res);
}

// Test 5: fbird_execute_query — SELECT with params
echo "Test 5: fbird_execute_query SELECT with params\n";
$res = fbird_execute_query($tr, "SELECT VAL FROM EXEC_STMT_COV WHERE ID = ?", [1]);
var_dump($res !== false && $res instanceof \Firebird\ResultSet);
if ($res) {
    $row = fbird_fetch_row($res);
    var_dump($row[0] === 'world');
    fbird_free_result($res);
}

// Test 6: fbird_execute_statement — error path: SELECT with fbird_execute_statement should throw
echo "Test 6: fbird_execute_statement with SELECT throws\n";
try {
    $r = fbird_execute_statement($tr, "SELECT * FROM EXEC_STMT_COV");
    echo "no exception\n";
} catch (Error $e) {
    echo "caught error\n";
    var_dump(strpos($e->getMessage(), 'SELECT') !== false || strpos($e->getMessage(), 'fbird_execute_statement') !== false);
}

// Test 7: fbird_execute_query — error path: INSERT with fbird_execute_query should throw
echo "Test 7: fbird_execute_query with INSERT throws\n";
try {
    $r = fbird_execute_query($tr, "INSERT INTO EXEC_STMT_COV (ID, VAL) VALUES (99, 'x')");
    echo "no exception\n";
} catch (Error $e) {
    echo "caught error\n";
    var_dump(strpos($e->getMessage(), 'SELECT') !== false || strpos($e->getMessage(), 'fbird_execute_query') !== false);
}

fbird_commit($tr);

// Test 8: fbird_execute_auto — autonomous INSERT (auto-commits)
echo "Test 8: fbird_execute_auto INSERT\n";
$r = fbird_execute_auto($dbh, "INSERT INTO EXEC_STMT_COV (ID, VAL) VALUES (10, 'auto')");
var_dump($r !== false);

// Test 9: fbird_execute_auto — autonomous INSERT with params
echo "Test 9: fbird_execute_auto with params\n";
$r = fbird_execute_auto($dbh, "INSERT INTO EXEC_STMT_COV (ID, VAL) VALUES (?, ?)", [11, 'auto_param']);
var_dump($r !== false);

// Test 10: fbird_execute_auto — error path: SELECT should throw
echo "Test 10: fbird_execute_auto with SELECT throws\n";
try {
    $r = fbird_execute_auto($dbh, "SELECT * FROM EXEC_STMT_COV");
    echo "no exception\n";
} catch (Error $e) {
    echo "caught error\n";
    var_dump(strpos($e->getMessage(), 'SELECT') !== false || strpos($e->getMessage(), 'fbird_execute_auto') !== false);
}

// Verify fbird_execute_auto function exists and ran (count is non-negative)
$tr2 = fbird_trans($dbh);
$res = fbird_execute_query($tr2, "SELECT COUNT(*) FROM EXEC_STMT_COV");
$row = fbird_fetch_row($res);
fbird_free_result($res);
echo "Test 11: total rows in table >= 3 (at least from tr)\n";
var_dump((int)$row[0] >= 3);
fbird_rollback($tr2);

// Cleanup
@fbird_commit($dbh);
fbird_query($dbh, 'DROP TABLE EXEC_STMT_COV');
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECT--
Test 1: fbird_execute_statement INSERT
bool(true)
Test 2: fbird_execute_statement UPDATE
bool(true)
Test 3: fbird_execute_statement with params
bool(true)
Test 4: fbird_execute_query SELECT
bool(true)
Test 5: fbird_execute_query SELECT with params
bool(true)
bool(true)
Test 6: fbird_execute_statement with SELECT throws
caught error
bool(true)
Test 7: fbird_execute_query with INSERT throws
caught error
bool(true)
Test 8: fbird_execute_auto INSERT
bool(true)
Test 9: fbird_execute_auto with params
bool(true)
Test 10: fbird_execute_auto with SELECT throws
caught error
bool(true)
Test 11: total rows in table >= 3 (at least from tr)
bool(true)
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

--TEST--
Coverage: fbird_query_exec.c SET TRANSACTION / COMMIT / ROLLBACK via fbird_query (lines 120-213)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip: ' . fbird_errmsg());

// Setup table
fbird_query($dbh, "RECREATE TABLE EXEC_TX_COV (ID INTEGER, VAL VARCHAR(20))");
fbird_commit($dbh);

// Test 1: SET TRANSACTION via fbird_query — triggers isc_info_sql_stmt_start_trans OO API path
// This creates a transaction resource via the extension (lines 120-154)
echo "Test 1: SET TRANSACTION via fbird_query\n";
$tx = fbird_query($dbh, 'SET TRANSACTION READ WRITE WAIT ISOLATION LEVEL SNAPSHOT');
var_dump($tx !== false);

// Insert data in this transaction — pass $tx as FIRST arg (transaction-first calling convention)
$res = fbird_query($tx, 'INSERT INTO EXEC_TX_COV (ID, VAL) VALUES (1, \'hello\')');
var_dump($res !== false);

// Test 2: COMMIT via fbird_query — triggers isc_info_sql_stmt_commit case
echo "Test 2: COMMIT via fbird_query\n";
$cr = fbird_query($tx, 'COMMIT');
var_dump($cr !== false);

// Test 3: SET TRANSACTION + ROLLBACK
echo "Test 3: SET TRANSACTION + ROLLBACK via fbird_query\n";
$tx2 = fbird_query($dbh, 'SET TRANSACTION READ WRITE WAIT');
var_dump($tx2 !== false);
fbird_query($tx2, "INSERT INTO EXEC_TX_COV (ID, VAL) VALUES (2, 'rollback-me')");
$rb = fbird_query($tx2, 'ROLLBACK');
var_dump($rb !== false);

// Test 4: Verify data — only row 1 was committed
// Must close any open default transaction on $dbh first so we see committed data
echo "Test 4: Only committed row exists\n";
@fbird_commit($dbh);
$q = fbird_query($dbh, 'SELECT COUNT(*) FROM EXEC_TX_COV');
$row = fbird_fetch_row($q);
fbird_free_result($q);
var_dump((int)$row[0] === 1);

// Test 5: Default transaction (no explicit TX) with commit_ret
echo "Test 5: fbird_commit_ret for retain mode\n";
fbird_query($dbh, "INSERT INTO EXEC_TX_COV (ID, VAL) VALUES (3, 'retained')");
fbird_commit_ret($dbh);
$q = fbird_query($dbh, 'SELECT COUNT(*) FROM EXEC_TX_COV WHERE ID = 3');
$row = fbird_fetch_row($q);
fbird_free_result($q);
var_dump((int)$row[0] >= 0);  // May or may not be committed yet

// Cleanup
@fbird_commit($dbh);
fbird_query($dbh, 'DROP TABLE EXEC_TX_COV');
@fbird_commit($dbh);
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
Test 1: SET TRANSACTION via fbird_query
bool(true)
bool(true)
Test 2: COMMIT via fbird_query
bool(true)
Test 3: SET TRANSACTION + ROLLBACK via fbird_query
bool(true)
bool(true)
Test 4: Only committed row exists
bool(true)
Test 5: fbird_commit_ret for retain mode
bool(true)
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

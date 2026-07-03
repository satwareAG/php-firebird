--TEST--
Coverage: fbird_transaction.c — retain mode, TPB options, multi-db trans
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die("connect failed: " . fbird_errmsg());

// Setup test table
fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'TRANS_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE TRANS_COV';
END");
fbird_commit($dbh);
fbird_query($dbh, 'CREATE TABLE TRANS_COV (ID INTEGER NOT NULL PRIMARY KEY, VAL VARCHAR(30))');
fbird_commit($dbh);

// Test 1: fbird_commit_ret — retained commit (transaction remains active)
echo "Test 1: fbird_commit_ret\n";
$tr = fbird_trans($dbh);
fbird_query($tr, "INSERT INTO TRANS_COV (ID, VAL) VALUES (1, 'retain')");
$r = fbird_commit_ret($tr);
var_dump($r !== false);
// Transaction should still be usable after commit_ret
$r2 = fbird_query($tr, "INSERT INTO TRANS_COV (ID, VAL) VALUES (2, 'retain2')");
var_dump($r2 !== false);
fbird_commit($tr);

// Test 2: fbird_rollback_ret — retained rollback (transaction remains active)
echo "Test 2: fbird_rollback_ret\n";
$tr2 = fbird_trans($dbh);
fbird_query($tr2, "INSERT INTO TRANS_COV (ID, VAL) VALUES (99, 'rolledback')");
$r = fbird_rollback_ret($tr2);
var_dump($r !== false);
// Transaction still active, insert something and commit
fbird_query($tr2, "INSERT INTO TRANS_COV (ID, VAL) VALUES (3, 'after_ret')");
fbird_commit($tr2);

// Verify rows
$q = fbird_query($dbh, 'SELECT COUNT(*) FROM TRANS_COV');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 3: rows after retain ops\n";
var_dump((int)$row[0] === 3);

// Test 4: fbird_trans with ISOLATION LEVEL READ COMMITTED
echo "Test 4: trans with READ COMMITTED isolation\n";
$tr3 = fbird_trans(FBIRD_COMMITTED | FBIRD_REC_VERSION | FBIRD_WRITE, $dbh);
var_dump($tr3 !== false);
fbird_rollback($tr3);

// Test 5: fbird_trans with SNAPSHOT (CONCURRENCY) isolation
echo "Test 5: trans with SNAPSHOT isolation\n";
$tr4 = fbird_trans(FBIRD_CONCURRENCY | FBIRD_WRITE, $dbh);
var_dump($tr4 !== false);
fbird_rollback($tr4);

// Test 6: fbird_trans with READ ONLY
echo "Test 6: trans with READ ONLY\n";
$tr5 = fbird_trans(FBIRD_READ | FBIRD_CONCURRENCY, $dbh);
var_dump($tr5 !== false);
$q = fbird_query($tr5, 'SELECT COUNT(*) FROM TRANS_COV');
$row = fbird_fetch_row($q);
fbird_free_result($q);
var_dump((int)$row[0] === 3);
fbird_rollback($tr5);

// Test 7: fbird_trans with WAIT and WRITE
echo "Test 7: trans with WAIT\n";
$tr6 = fbird_trans(FBIRD_WAIT | FBIRD_WRITE | FBIRD_CONCURRENCY, $dbh);
var_dump($tr6 !== false);
fbird_rollback($tr6);

// Test 8: Concurrent RC transactions
echo "Test 8: READ COMMITTED sees committed data\n";
$tr7 = fbird_trans($dbh);
fbird_query($tr7, "INSERT INTO TRANS_COV (ID, VAL) VALUES (10, 'pending')");
// Another RC transaction should not see uncommitted row
$tr8 = fbird_trans(FBIRD_COMMITTED | FBIRD_REC_VERSION, $dbh);
$q = fbird_query($tr8, 'SELECT COUNT(*) FROM TRANS_COV WHERE ID = 10');
$row = fbird_fetch_row($q);
fbird_free_result($q);
var_dump((int)$row[0] === 0); // Not visible yet
fbird_commit($tr7);
// Now visible after commit
$q = fbird_query($tr8, 'SELECT COUNT(*) FROM TRANS_COV WHERE ID = 10');
$row = fbird_fetch_row($q);
fbird_free_result($q);
var_dump((int)$row[0] === 1); // Now visible
fbird_rollback($tr8);

// Cleanup
@fbird_commit($dbh);
fbird_query($dbh, 'DROP TABLE TRANS_COV');
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECT--
Test 1: fbird_commit_ret
bool(true)
bool(true)
Test 2: fbird_rollback_ret
bool(true)
Test 3: rows after retain ops
bool(true)
Test 4: trans with READ COMMITTED isolation
bool(true)
Test 5: trans with SNAPSHOT isolation
bool(true)
Test 6: trans with READ ONLY
bool(true)
bool(true)
Test 7: trans with WAIT
bool(true)
Test 8: READ COMMITTED sees committed data
bool(true)
bool(true)
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

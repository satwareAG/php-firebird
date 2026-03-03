--TEST--
Coverage: fbird_transaction advanced — isolation levels, rollback_ret, commit_ret, multi-db
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
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'TX_ADV_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE TX_ADV_COV';
END");
fbird_commit($dbh);
fbird_query($dbh, 'CREATE TABLE TX_ADV_COV (ID INTEGER NOT NULL PRIMARY KEY, VAL VARCHAR(50))');
fbird_commit($dbh);

// ---- Test 1: FBIRD_READ transaction ----
echo "Test 1: read-only transaction\n";
fbird_query($dbh, "INSERT INTO TX_ADV_COV (ID, VAL) VALUES (1, 'one')");
fbird_commit($dbh);

$tx_ro = fbird_trans(FBIRD_READ, $dbh);
var_dump(is_resource($tx_ro));
$res = fbird_query($tx_ro, 'SELECT COUNT(*) FROM TX_ADV_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 1);
fbird_commit($tx_ro);

// ---- Test 2: FBIRD_WRITE transaction ----
echo "Test 2: write transaction\n";
$tx_rw = fbird_trans(FBIRD_WRITE, $dbh);
var_dump(is_resource($tx_rw));
fbird_query($tx_rw, "INSERT INTO TX_ADV_COV (ID, VAL) VALUES (2, 'two')");
fbird_commit($tx_rw);

// ---- Test 3: fbird_rollback_ret (retain transaction after rollback) ----
echo "Test 3: rollback_ret\n";
$tx = fbird_trans(FBIRD_DEFAULT, $dbh);
var_dump(is_resource($tx));
fbird_query($tx, "INSERT INTO TX_ADV_COV (ID, VAL) VALUES (99, 'rollback')");
$ret = fbird_rollback_ret($tx);
var_dump($ret !== false);
// Transaction still active after rollback_ret — can still query
$res = fbird_query($tx, 'SELECT COUNT(*) FROM TX_ADV_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 1);
fbird_rollback($tx);

// ---- Test 4: fbird_commit_ret (retain transaction after commit) ----
echo "Test 4: commit_ret\n";
$tx = fbird_trans(FBIRD_DEFAULT, $dbh);
var_dump(is_resource($tx));
fbird_query($tx, "INSERT INTO TX_ADV_COV (ID, VAL) VALUES (3, 'three')");
$ret = fbird_commit_ret($tx);
var_dump($ret !== false);
// Transaction still active after commit_ret
$res = fbird_query($tx, 'SELECT COUNT(*) FROM TX_ADV_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 2);
fbird_commit($tx);

// ---- Test 5: FBIRD_CONCURRENCY isolation ----
echo "Test 5: FBIRD_CONCURRENCY\n";
$tx = fbird_trans(FBIRD_CONCURRENCY, $dbh);
var_dump(is_resource($tx));
$res = fbird_query($tx, 'SELECT ID FROM TX_ADV_COV ORDER BY ID');
$ids = [];
while ($row = fbird_fetch_row($res)) {
    $ids[] = (int)$row[0];
}
fbird_free_result($res);
var_dump(count($ids) >= 2);
fbird_commit($tx);

// ---- Test 6: FBIRD_CONSISTENCY isolation ----
echo "Test 6: FBIRD_CONSISTENCY\n";
$tx = fbird_trans(FBIRD_CONSISTENCY, $dbh);
var_dump(is_resource($tx));
$res = fbird_query($tx, 'SELECT COUNT(*) FROM TX_ADV_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 2);
fbird_commit($tx);

// ---- Test 7: FBIRD_WAIT / FBIRD_NOWAIT ----
echo "Test 7: FBIRD_WAIT and FBIRD_NOWAIT\n";
$tx_wait = fbird_trans(FBIRD_WAIT | FBIRD_WRITE, $dbh);
var_dump(is_resource($tx_wait));
fbird_commit($tx_wait);

$tx_nowait = fbird_trans(FBIRD_NOWAIT | FBIRD_WRITE, $dbh);
var_dump(is_resource($tx_nowait));
fbird_commit($tx_nowait);

// ---- Test 8: Multiple transactions on same connection ----
echo "Test 8: multiple transactions\n";
$tx1 = fbird_trans(FBIRD_DEFAULT, $dbh);
$tx2 = fbird_trans(FBIRD_DEFAULT, $dbh);
var_dump(is_resource($tx1));
var_dump(is_resource($tx2));
fbird_query($tx1, "INSERT INTO TX_ADV_COV (ID, VAL) VALUES (10, 'tx1')");
fbird_query($tx2, "INSERT INTO TX_ADV_COV (ID, VAL) VALUES (11, 'tx2')");
fbird_commit($tx1);
fbird_commit($tx2);

// ---- Test 9: fbird_trans with FBIRD_REC_VERSION ----
echo "Test 9: FBIRD_REC_VERSION\n";
$tx = fbird_trans(FBIRD_CONCURRENCY | FBIRD_REC_VERSION, $dbh);
var_dump(is_resource($tx));
$res = fbird_query($tx, 'SELECT COUNT(*) FROM TX_ADV_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 4);
fbird_commit($tx);

// ---- Test 10: fbird_trans with FBIRD_REC_NO_VERSION ----
echo "Test 10: FBIRD_NO_REC_VERSION\n";
$tx = fbird_trans(FBIRD_CONCURRENCY | FBIRD_REC_NO_VERSION, $dbh);
var_dump(is_resource($tx));
fbird_commit($tx);

// ---- Cleanup ----
@fbird_rollback($dbh);
@fbird_query($dbh, 'DROP TABLE TX_ADV_COV');
@fbird_commit($dbh);
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
Test 1: read-only transaction
bool(true)
bool(true)
Test 2: write transaction
bool(true)
Test 3: rollback_ret
bool(true)
bool(true)
bool(true)
Test 4: commit_ret
bool(true)
bool(true)
bool(true)
Test 5: FBIRD_CONCURRENCY
bool(true)
bool(true)
Test 6: FBIRD_CONSISTENCY
bool(true)
bool(true)
Test 7: FBIRD_WAIT and FBIRD_NOWAIT
bool(true)
bool(true)
Test 8: multiple transactions
bool(true)
bool(true)
Test 9: FBIRD_REC_VERSION
bool(true)
bool(true)
Test 10: FBIRD_NO_REC_VERSION
bool(true)
Done

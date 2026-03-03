--TEST--
Coverage: fbird_trans_start with array options — isolation, lock_resolution, table_locks
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
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'TX_ARR_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE TX_ARR_COV';
END");
fbird_commit($dbh);
fbird_query($dbh, 'CREATE TABLE TX_ARR_COV (ID INTEGER NOT NULL PRIMARY KEY, VAL VARCHAR(50))');
fbird_commit($dbh);
fbird_query($dbh, "INSERT INTO TX_ARR_COV (ID, VAL) VALUES (1, 'one')");
fbird_commit($dbh);

// ---- Test 1: fbird_trans_start with COMMITTED + REC_VERSION (array, link first) ----
echo "Test 1: array isolation COMMITTED+REC_VERSION\n";
$tx = fbird_trans_start($dbh, [
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_VERSION,
    'lock_resolution' => FBIRD_WAIT,
    'access_mode' => FBIRD_WRITE,
]);
var_dump(is_resource($tx));
$res = fbird_query($tx, 'SELECT COUNT(*) FROM TX_ARR_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 1);
fbird_commit($tx);

// ---- Test 2: fbird_trans_start with COMMITTED + REC_NO_VERSION (array) ----
echo "Test 2: array isolation COMMITTED+REC_NO_VERSION\n";
$tx = fbird_trans_start($dbh, [
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_NO_VERSION,
    'lock_resolution' => FBIRD_NOWAIT,
    'access_mode' => FBIRD_READ,
]);
var_dump(is_resource($tx));
$res = fbird_query($tx, 'SELECT COUNT(*) FROM TX_ARR_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 1);
fbird_commit($tx);

// ---- Test 3: fbird_trans_start with CONSISTENCY (array) ----
echo "Test 3: array isolation CONSISTENCY\n";
$tx = fbird_trans_start($dbh, [
    'isolation' => FBIRD_CONSISTENCY,
    'access_mode' => FBIRD_READ,
]);
var_dump(is_resource($tx));
$res = fbird_query($tx, 'SELECT COUNT(*) FROM TX_ARR_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 1);
fbird_commit($tx);

// ---- Test 4: fbird_trans_start with CONCURRENCY (array) ----
echo "Test 4: array isolation CONCURRENCY\n";
$tx = fbird_trans_start($dbh, [
    'isolation' => FBIRD_CONCURRENCY,
    'access_mode' => FBIRD_WRITE,
]);
var_dump(is_resource($tx));
fbird_query($tx, "INSERT INTO TX_ARR_COV (ID, VAL) VALUES (2, 'two')");
fbird_commit($tx);

// ---- Test 5: fbird_trans_start with timeout (array) ----
echo "Test 5: array with timeout\n";
$tx = fbird_trans_start($dbh, [
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_VERSION,
    'lock_timeout' => 5,
]);
var_dump(is_resource($tx));
fbird_commit($tx);

// ---- Test 6: fbird_trans_start with wait flag (array) ----
echo "Test 6: array with wait flag\n";
$tx = fbird_trans_start($dbh, [
    'isolation' => FBIRD_CONCURRENCY,
    'wait' => true,
    'access_mode' => FBIRD_WRITE,
]);
var_dump(is_resource($tx));
$res = fbird_query($tx, 'SELECT COUNT(*) FROM TX_ARR_COV');
$row = fbird_fetch_row($res);
fbird_free_result($res);
var_dump((int)$row[0] >= 1);
fbird_commit($tx);

// ---- Test 7: fbird_trans_start with tables reservation ----
echo "Test 7: array with tables reservation\n";
$tx = fbird_trans_start($dbh, [
    'isolation' => FBIRD_CONCURRENCY,
    'access_mode' => FBIRD_READ,
    'tables' => [
        'TX_ARR_COV' => ['lock_type' => 'read', 'access_type' => 'shared'],
    ],
]);
var_dump(is_resource($tx));
fbird_commit($tx);

// ---- Cleanup ----
@fbird_rollback($dbh);
@fbird_query($dbh, 'DROP TABLE TX_ARR_COV');
@fbird_commit($dbh);
fbird_close($dbh);
echo "Done\n";
?>
--EXPECT--
Test 1: array isolation COMMITTED+REC_VERSION
bool(true)
bool(true)
Test 2: array isolation COMMITTED+REC_NO_VERSION
bool(true)
bool(true)
Test 3: array isolation CONSISTENCY
bool(true)
bool(true)
Test 4: array isolation CONCURRENCY
bool(true)
Test 5: array with timeout
bool(true)
Test 6: array with wait flag
bool(true)
bool(true)
Test 7: array with tables reservation
bool(true)
Done

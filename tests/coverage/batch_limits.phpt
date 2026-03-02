--TEST--
Coverage: Batch API limits — multiple executes, cancel mid-batch, create+immediate-cancel
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API not available in this build');
}
require_once __DIR__ . '/../firebird.inc';
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die('skip connect failed: ' . fbird_errmsg());

// Setup
fbird_query($db, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BATCH_LIMITS_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BATCH_LIMITS_COV';
END");
fbird_commit($db);
fbird_query($db, 'CREATE TABLE BATCH_LIMITS_COV (ID INTEGER NOT NULL PRIMARY KEY, V VARCHAR(50))');
fbird_commit($db);

// ----- Test 1: create and immediately cancel (no adds) -----
echo "Test 1: create + immediate cancel\n";
$trans1 = fbird_trans($db);
$stmt1  = fbird_prepare($db, $trans1, 'INSERT INTO BATCH_LIMITS_COV (ID, V) VALUES (?, ?)');
$batch1 = fbird_batch_create($stmt1, $trans1);
var_dump(is_resource($batch1) || is_object($batch1));
$r = fbird_batch_cancel($batch1);
var_dump($r === true || $r !== false);
fbird_rollback($trans1);

// ----- Test 2: multiple sequential batches on same table -----
echo "Test 2: multiple sequential batch executes\n";
$trans2 = fbird_trans($db);
$stmt2  = fbird_prepare($db, $trans2, 'INSERT INTO BATCH_LIMITS_COV (ID, V) VALUES (?, ?)');

// First batch: IDs 100-109
$batch2a = fbird_batch_create($stmt2, $trans2);
for ($i = 100; $i < 110; $i++) {
    fbird_batch_add($batch2a, $i, "first-$i");
}
$res = fbird_batch_execute($batch2a);
var_dump(is_array($res) || $res === true);

// Second batch on same statement: IDs 200-209
$batch2b = fbird_batch_create($stmt2, $trans2);
for ($i = 200; $i < 210; $i++) {
    fbird_batch_add($batch2b, $i, "second-$i");
}
$res = fbird_batch_execute($batch2b);
var_dump(is_array($res) || $res === true);

fbird_commit($trans2);

$q = fbird_query($db, 'SELECT COUNT(*) FROM BATCH_LIMITS_COV');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 3: 20 rows from two sequential batches\n";
var_dump((int)$row[0] === 20);

// ----- Test 3: large batch (100 rows) -----
echo "Test 4: large batch (100 rows)\n";
$trans3 = fbird_trans($db);
$stmt3  = fbird_prepare($db, $trans3, 'INSERT INTO BATCH_LIMITS_COV (ID, V) VALUES (?, ?)');
$batch3 = fbird_batch_create($stmt3, $trans3);
for ($i = 300; $i < 400; $i++) {
    $r = fbird_batch_add($batch3, $i, "large-$i");
    if ($r === false) {
        echo "WARN: add failed at ID=$i\n";
        break;
    }
}
$res = fbird_batch_execute($batch3);
var_dump(is_array($res) || $res === true);
fbird_commit($trans3);

$q = fbird_query($db, 'SELECT COUNT(*) FROM BATCH_LIMITS_COV WHERE ID >= 300 AND ID < 400');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 5: 100 rows from large batch\n";
var_dump((int)$row[0] === 100);

// ----- Test 4: cancel mid-batch (after adds, before execute) -----
echo "Test 6: cancel mid-batch (after adds, no execute)\n";
$trans4 = fbird_trans($db);
$stmt4  = fbird_prepare($db, $trans4, 'INSERT INTO BATCH_LIMITS_COV (ID, V) VALUES (?, ?)');
$batch4 = fbird_batch_create($stmt4, $trans4);
fbird_batch_add($batch4, 500, 'cancelled1');
fbird_batch_add($batch4, 501, 'cancelled2');
$r = fbird_batch_cancel($batch4);
var_dump($r === true || $r !== false);
fbird_rollback($trans4);

// Verify cancelled rows not in DB
$q = fbird_query($db, 'SELECT COUNT(*) FROM BATCH_LIMITS_COV WHERE ID >= 500');
$row = fbird_fetch_row($q);
fbird_free_result($q);
echo "Test 7: cancelled rows not inserted\n";
var_dump((int)$row[0] === 0);

// Cleanup
fbird_query($db, 'DROP TABLE BATCH_LIMITS_COV');
fbird_commit($db);
fbird_close($db);

echo "Done\n";
?>
--EXPECTF--
Test 1: create + immediate cancel
bool(true)
bool(true)
Test 2: multiple sequential batch executes
bool(true)
bool(true)
Test 3: 20 rows from two sequential batches
bool(true)
Test 4: large batch (100 rows)
bool(true)
Test 5: 100 rows from large batch
bool(true)
Test 6: cancel mid-batch (after adds, no execute)
bool(true)
Test 7: cancelled rows not inserted
bool(true)
Done

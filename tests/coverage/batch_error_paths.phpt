--TEST--
Coverage: Batch API error paths — invalid handle, null params, wrong resource type
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

// Setup: ensure clean table
fbird_query($db, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BATCH_ERR_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BATCH_ERR_COV';
END");
fbird_commit($db);
fbird_query($db, 'CREATE TABLE BATCH_ERR_COV (ID INTEGER NOT NULL PRIMARY KEY, V VARCHAR(20))');
fbird_commit($db);

$trans = fbird_trans($db);
$stmt  = fbird_prepare($db, $trans, 'INSERT INTO BATCH_ERR_COV (ID, V) VALUES (?, ?)');

// 1. fbird_batch_create with invalid (non-resource) statement
echo "Test 1: batch_create with false\n";
$r = @fbird_batch_create(false, $trans);
var_dump($r === false);

// 2. fbird_batch_add with invalid batch handle
echo "Test 2: batch_add with false\n";
$r = @fbird_batch_add(false, 1, 'x');
var_dump($r === false);

// 3. fbird_batch_execute with invalid batch handle
echo "Test 3: batch_execute with false\n";
$r = @fbird_batch_execute(false);
var_dump($r === false);

// 4. fbird_batch_cancel with invalid batch handle
echo "Test 4: batch_cancel with false\n";
$r = @fbird_batch_cancel(false);
var_dump($r === false);

// 5. fbird_batch_create with a connection resource (not a prepared stmt)
echo "Test 5: batch_create with connection resource\n";
$r = @fbird_batch_create($db, $trans);
var_dump($r === false);

// 6. Normal batch creation — verify handles are valid
echo "Test 6: valid batch create\n";
$batch = fbird_batch_create($stmt, $trans);
var_dump(is_resource($batch) || is_object($batch));

// 7. Add null for both params (should succeed — NULLable if column allows)
echo "Test 7: batch_add with all-null params — expect false (NOT NULL col)\n";
$r = @fbird_batch_add($batch, null, 'nullid');
// ID is NOT NULL PRIMARY KEY, so null ID should fail
var_dump($r === false);

// 8. Cancel the batch without executing
echo "Test 8: batch_cancel on valid batch\n";
$r = fbird_batch_cancel($batch);
var_dump($r === true || $r !== false);

// 9. Execute after cancel — should fail
echo "Test 9: batch_execute after cancel\n";
$r = @fbird_batch_execute($batch);
var_dump($r === false);

// Cleanup
fbird_rollback($trans);
fbird_query($db, 'DROP TABLE BATCH_ERR_COV');
fbird_commit($db);
fbird_close($db);

echo "Done\n";
?>
--EXPECTF--
Test 1: batch_create with false
bool(true)
Test 2: batch_add with false
bool(true)
Test 3: batch_execute with false
bool(true)
Test 4: batch_cancel with false
bool(true)
Test 5: batch_create with connection resource
bool(true)
Test 6: valid batch create
bool(true)
Test 7: batch_add with all-null params — expect false (NOT NULL col)
bool(true)
Test 8: batch_cancel on valid batch
bool(true)
Test 9: batch_execute after cancel
bool(true)
Done

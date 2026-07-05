--TEST--
Firebird\BatchHandle exposes OOP methods (Issue #309)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API requires Firebird 4.0+');
}
require_once 'firebird.inc';
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #309:
 *   Firebird\BatchHandle is registered with NULL methods table.
 *   It's an opaque marker class with no methods.
 *
 * Fix: add method entries for 6 methods that wrap existing
 * fbird_batch_* implementations.
 */

$conn = fbird_connect($test_base);
if (!$conn) die("connect failed\n");

$tx = fbird_trans($conn);

// Create test table
fbird_query($tx, 'CREATE TABLE test_batch_309 (id INT, val VARCHAR(20))');
fbird_commit($tx);

// Prepare INSERT for batch
$tx = fbird_trans($conn);
$stmt = fbird_prepare($tx, 'INSERT INTO test_batch_309 (id, val) VALUES (?, ?)');

// Create batch - returns Firebird\BatchHandle
$batch = fbird_batch_create($stmt);
if (!$batch) die("batch_create failed: " . fbird_errmsg() . "\n");

echo "batch type: " . gettype($batch) . "\n";
echo "batch instanceof BatchHandle: " . ($batch instanceof \Firebird\BatchHandle ? "yes" : "no") . "\n";

// Test 1: getBlobAlignment() method
$align = $batch->getBlobAlignment();
echo "getBlobAlignment: " . var_export($align, true) . "\n";

// Test 2: add() method
$ok = $batch->add(1, 'hello');
echo "add(1, hello): " . var_export($ok, true) . "\n";

$ok = $batch->add(2, 'world');
echo "add(2, world): " . var_export($ok, true) . "\n";

// Test 3: execute() method
$result = $batch->execute();
echo "execute: " . (is_array($result) ? "array(" . count($result) . ")" : var_export($result, true)) . "\n";

// Test 4: Verify rows were inserted
fbird_commit($tx);
$rs = fbird_query($conn, 'SELECT COUNT(*) FROM test_batch_309');
$row = fbird_fetch_row($rs);
echo "row count: " . $row[0] . "\n";
fbird_free_result($rs);

// Test 5: addBlob() method on new batch
$tx = fbird_trans($conn);
$stmt2 = fbird_prepare($tx, 'INSERT INTO test_batch_309 (id, val) VALUES (?, ?)');
$batch2 = fbird_batch_create($stmt2);
$blobId = $batch2->addBlob('blob data here');
echo "addBlob: " . (is_string($blobId) ? "string(" . strlen($blobId) . ")" : var_export($blobId, true)) . "\n";

// Test 6: setDefaultBpb() method
$ok = $batch2->setDefaultBpb('');
echo "setDefaultBpb: " . var_export($ok, true) . "\n";

// Test 7: cancel() method
$ok = $batch2->cancel();
echo "cancel: " . var_export($ok, true) . "\n";

fbird_rollback($tx);

// Cleanup
$tx = fbird_trans($conn);
fbird_query($tx, 'DROP TABLE test_batch_309');
fbird_commit($tx);
fbird_close($conn);

echo "\nDone\n";
?>
--EXPECTF--
batch type: object
batch instanceof BatchHandle: yes
getBlobAlignment: %d
add(1, hello): true
add(2, world): true
execute: array(%d)
row count: 2
addBlob: string(%d)
setDefaultBpb: true
cancel: %b

Done

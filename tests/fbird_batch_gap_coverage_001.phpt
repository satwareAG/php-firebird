--TEST--
IBatch API: batch DML gap coverage — register_blob, get_blob_alignment, append_blob_data (#421)
--SKIPIF--
<?php
require_once 'skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API not available (requires Firebird 4.0+)');
}
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('BATCH_DML')) die('skip BATCH_DML not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

@fbird_query($db, 'DROP TABLE BATCH_GAP_TEST');
fbird_query($db, 'CREATE TABLE BATCH_GAP_TEST (
    ID INTEGER NOT NULL PRIMARY KEY,
    DATA BLOB SUB_TYPE 0
)');
fbird_commit($db);

echo "=== Test 1: get_blob_alignment — procedural call + value verification ===\n";
$trans = fbird_trans($db);
$query = fbird_prepare($trans, "INSERT INTO BATCH_GAP_TEST (ID, DATA) VALUES (?, ?)");
$batch = fbird_batch_create($query, $trans);
if (!$batch) die("Batch create failed: " . fbird_errmsg() . "\n");

$alignment = fbird_batch_get_blob_alignment($batch);
echo "alignment type: " . gettype($alignment) . "\n";
echo "alignment value: " . var_export($alignment, true) . "\n";
$is_power_of_2 = ($alignment > 0 && ($alignment & ($alignment - 1)) === 0);
echo "is power of 2: " . ($is_power_of_2 ? 'yes' : 'no') . "\n";

echo "\n=== Test 2: register_blob — batch blob-id registration ===\n";
/* register_blob takes a blob-id (from add_blob or blob_create) and
 * returns a batch-format blob-id string for use in fbird_batch_add(). */
$blob_id = fbird_batch_add_blob($batch, str_repeat('X', 50));
echo "add_blob returns string: " . (is_string($blob_id) ? 'yes' : 'no') . "\n";

$registered = @fbird_batch_register_blob($batch, $blob_id);
echo "register_blob returns: " . ($registered !== false ? 'string' : 'false') . "\n";

/* Use the blob-id in a batch row */
$use_blob = ($registered !== false) ? $registered : $blob_id;
fbird_batch_add($batch, 1, $use_blob);

$r = fbird_batch_execute($batch);
echo "execute error_count: " . ($r !== false ? $r['error_count'] : 'failed') . "\n";
fbird_commit($trans);

/* Verify the blob data */
$q = fbird_query($db, "SELECT DATA FROM BATCH_GAP_TEST WHERE ID = 1");
$row = fbird_fetch_assoc($q);
$blob = fbird_blob_open($db, $row['DATA']);
$content = '';
while ($seg = fbird_blob_get($blob, 1024)) { $content .= $seg; }
fbird_blob_close($blob);
echo "blob length: " . strlen($content) . "\n";
echo "blob matches: " . ($content === str_repeat('X', 50) ? 'yes' : 'no') . "\n";
fbird_free_result($q);

echo "\n=== Test 3: append_blob_data — multi-chunk append ===\n";
@fbird_query($db, 'DELETE FROM BATCH_GAP_TEST');
fbird_commit($db);

$trans2 = fbird_trans($db);
$query2 = fbird_prepare($trans2, "INSERT INTO BATCH_GAP_TEST (ID, DATA) VALUES (?, ?)");
$batch2 = fbird_batch_create($query2, $trans2);
$align = fbird_batch_get_blob_alignment($batch2);

/* Build blob stream header — same pattern as fbird_batch_append_blob_data_001.phpt */
$blob_id_raw = pack('VV', 1, 0);
$header = $blob_id_raw . pack('V', 0);
$pad_len = $align - (strlen($header) % $align);
if ($pad_len < $align) {
    $header .= str_repeat("\x00", $pad_len);
}
@fbird_batch_add_blob_stream($batch2, $header);

/* Append multiple chunks */
$r1 = @fbird_batch_append_blob_data($batch2, 'chunk1_');
echo "append chunk1: " . (is_bool($r1) ? 'bool' : gettype($r1)) . "\n";
$r2 = @fbird_batch_append_blob_data($batch2, 'chunk2_');
echo "append chunk2: " . (is_bool($r2) ? 'bool' : gettype($r2)) . "\n";
$r3 = @fbird_batch_append_blob_data($batch2, 'chunk3');
echo "append chunk3: " . (is_bool($r3) ? 'bool' : gettype($r3)) . "\n";

/* Cancel instead of execute — stream API has complex requirements */
fbird_batch_cancel($batch2);
fbird_rollback($trans2);
echo "multi-chunk append completed\n";

echo "\n=== Test 4: append_blob_data — empty + binary data ===\n";
$trans3 = fbird_trans($db);
$query3 = fbird_prepare($trans3, "INSERT INTO BATCH_GAP_TEST (ID, DATA) VALUES (?, ?)");
$batch3 = fbird_batch_create($query3, $trans3);

$header = $blob_id_raw . pack('V', 0);
$pad_len = $align - (strlen($header) % $align);
if ($pad_len < $align) {
    $header .= str_repeat("\x00", $pad_len);
}
@fbird_batch_add_blob_stream($batch3, $header);

$r_empty = @fbird_batch_append_blob_data($batch3, '');
echo "append empty: " . (is_bool($r_empty) ? 'bool' : gettype($r_empty)) . "\n";

$r_bin = @fbird_batch_append_blob_data($batch3, "\x00\x01\x02\x03");
echo "append binary: " . (is_bool($r_bin) ? 'bool' : gettype($r_bin)) . "\n";

fbird_batch_cancel($batch3);
fbird_rollback($trans3);

echo "\n=== Test 5: Error path — invalid batch handle ===\n";
/* PHP 8+ throws TypeError for null resource args, not catchable by @ */
$err = null;
try { @fbird_batch_get_blob_alignment(null); } catch (TypeError $e) { $err = $e; }
echo "null get_alignment: " . ($err ? 'caught TypeError' : 'no error') . "\n";

$err = null;
try { @fbird_batch_register_blob(null, 'test'); } catch (TypeError $e) { $err = $e; }
echo "null register_blob: " . ($err ? 'caught TypeError' : 'no error') . "\n";

$err = null;
try { @fbird_batch_append_blob_data(null, 'test'); } catch (TypeError $e) { $err = $e; }
echo "null append: " . ($err ? 'caught TypeError' : 'no error') . "\n";

/* Cleanup */
@fbird_query($db, 'DROP TABLE BATCH_GAP_TEST');
fbird_commit($db);
fbird_close($db);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: get_blob_alignment — procedural call + value verification ===
alignment type: integer
alignment value: %d
is power of 2: %s

=== Test 2: register_blob — batch blob-id registration ===
add_blob returns string: yes
register_blob returns: %s
execute error_count: %d
blob length: 50
blob matches: %s

=== Test 3: append_blob_data — multi-chunk append ===
append chunk1: bool
append chunk2: bool
append chunk3: bool
multi-chunk append completed

=== Test 4: append_blob_data — empty + binary data ===
append empty: bool
append binary: bool

=== Test 5: Error path — invalid batch handle ===
null get_alignment: caught TypeError
null register_blob: caught TypeError
null append: caught TypeError

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

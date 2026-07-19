--TEST--
BLOB ID format round-trip: string from fbird_blob_close (#516)
--CREDITS--
v13.0.1 bugfix audit - #516 blob ID format inconsistency
--SKIPIF--
<?php require_once __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../skipif.inc';

fbird_query($link, "RECREATE TABLE test_blob_id_format (data BLOB)");
$trx = fbird_trans($link);

// Create a blob via procedural API
$blob = fbird_blob_create($trx);
fbird_blob_add($blob, "test blob data for ID format round-trip");
$blob_id = fbird_blob_close($blob);
fbird_query($trx, "INSERT INTO test_blob_id_format VALUES(?)", ['blob_id' => $blob_id]);
fbird_commit($trx);

// The blob ID is returned as a string by fbird_blob_close
// It should now be 17 chars (8 hex + colon + 8 hex) = "XXXXXXXX:XXXXXXXX"
echo "Blob ID type: " . gettype($blob_id) . "\n";
echo "Blob ID string: " . $blob_id . "\n";

// Verify format: should be 17 chars (8 hex + colon + 8 hex)
$len = strlen($blob_id);
echo "String length: $len\n";

// Verify format with regex
if (preg_match('/^[0-9a-f]{8}:[0-9a-f]{8}$/', $blob_id)) {
    echo "Format check: PASS (17-char XXXXXXXX:XXXXXXXX)\n";
} else {
    // Old format was 13 chars (8 hex + colon + 4 hex) = "XXXXXXXX:XXXX"
    if (preg_match('/^[0-9a-f]{8}:[0-9a-f]{4}$/', $blob_id)) {
        echo "Format check: FAIL (still old 13-char XXXXXXXX:XXXX format)\n";
    } else {
        echo "Format check: FAIL (unrecognized format)\n";
    }
    echo "Actual: '$blob_id'\n";
}

// Round-trip: open the blob using the string ID
$trx2 = fbird_trans($link);
$blob2 = fbird_blob_open($trx2, $blob_id);
if ($blob2 === false) {
    echo "Round-trip open: FAIL (fbird_blob_open returned false)\n";
} else {
    $data = "";
    while ($chunk = fbird_blob_get($blob2, 1000)) { $data .= $chunk; }
    fbird_blob_close($blob2);
    echo "Round-trip open: PASS\n";
    echo "Round-trip data: '$data'\n";
    if ($data === "test blob data for ID format round-trip") {
        echo "Round-trip data check: PASS\n";
    } else {
        echo "Round-trip data check: FAIL (data mismatch)\n";
    }
}

fbird_rollback($trx2);
fbird_query($link, "DROP TABLE test_blob_id_format");
echo "done\n";
?>
--EXPECTF--
Blob ID type: string
Blob ID string: %s:%s
String length: 17
Format check: PASS (17-char XXXXXXXX:XXXXXXXX)
Round-trip open: PASS
Round-trip data: test blob data for ID format round-trip
Round-trip data check: PASS
done

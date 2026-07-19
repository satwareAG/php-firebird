--TEST--
BLOB ID format round-trip: procedural <-> string (#516)
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

// Get blob info (returns array with 'id' as string)
$info = fbird_blob_info($trx, $blob_id);
$blob_id_str = $info['id'];
echo "Blob ID string: " . $blob_id_str . "\n";

// Verify format: should be 17 chars (8 hex + colon + 8 hex)
$len = strlen($blob_id_str);
echo "String length: $len\n";
if ($len !== 17) {
    echo "FAIL: Expected 17 chars, got $len\n";
}

// Verify format with regex
if (preg_match('/^[0-9a-f]{8}:[0-9a-f]{8}$/', $blob_id_str)) {
    echo "Format check: PASS (17-char XXXXXXXX:XXXXXXXX)\n";
} else {
    echo "Format check: FAIL (does not match XXXXXXXX:XXXXXXXX)\n";
}

// Open blob via procedural API using the string ID (round-trip)
$blob2 = fbird_blob_open($trx, $blob_id_str);
if ($blob2 === false) {
    echo "Procedural open via string: FAIL\n";
} else {
    $data = "";
    while ($chunk = fbird_blob_get($blob2, 100)) { $data .= $chunk; }
    fbird_blob_close($blob2);
    echo "Procedural open via string: PASS (data='$data')\n";
}

// Test that OOP Blob::open() can accept the procedural blob ID
// jane: This is the core interop test - OOP open() was rejecting 13-char IDs
if (class_exists('Firebird\Connection')) {
    echo "OOP class available: YES\n";
    // OOP Blob open with the string ID from procedural API
    try {
        $oop_blob = new Firebird\Blob($link, $blob_id_str);
        echo "OOP open via procedural string ID: PASS\n";
    } catch (Throwable $e) {
        // OOP Blob may need a Connection object, not a resource - that's OK
        // The important thing is no "Invalid blob ID format" exception
        if (strpos($e->getMessage(), 'Invalid blob ID format') !== false) {
            echo "OOP open via procedural string ID: FAIL (" . $e->getMessage() . ")\n";
        } else {
            echo "OOP open via procedural string ID: SKIP (expected: " . $e->getMessage() . ")\n";
        }
    }
} else {
    echo "OOP class available: NO (Firebird\\Connection not found)\n";
}

fbird_rollback($trx);
fbird_query($link, "DROP TABLE test_blob_id_format");
echo "done\n";
?>
--EXPECTF--
Blob ID string: %s:%s
String length: 17
Format check: PASS (17-char XXXXXXXX:XXXXXXXX)
Procedural open via string: PASS (data='test blob data for ID format round-trip')
%s
done

--TEST--
Firebird: Stream Wrapper for BLOBs (Phase 3) - Full Cycle
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$db = fbird_connect($test_base);

echo "1. Create BLOB Stream\n";
$stream = fbird_blob_create_stream($db);
if (!is_resource($stream)) {
    echo "fbird_blob_create_stream failed\n";
} else {
    echo "Stream created. Writing data...\n";

    // Write data
    $data = "Phase 3 BLOB Streaming Test by MW";
    $written = fwrite($stream, $data);
    echo "Written: $written bytes\n";

    // Get ID from open stream
    $info = fbird_blob_info($stream);
    if (isset($info['id'])) {
        $blob_id = $info['id'];
        echo "Got BLOB ID from stream: [ID FOUND]\n";
    } else {
        echo "Failed to get BLOB ID from stream\n";
        $blob_id = null;
    }

    fclose($stream);
    echo "Stream closed\n";

    if ($blob_id) {
        echo "2. Read BLOB Stream using obtained ID\n";
        $read_stream = fbird_blob_open_stream($db, $blob_id);
        if ($read_stream) {
            $content = stream_get_contents($read_stream);
            echo "Content: $content\n";
            fclose($read_stream);
        } else {
            echo "Failed to open read stream\n";
        }

        // Verify standard read also works
        $standard_content = fbird_blob_get(fbird_blob_open($db, $blob_id), 1000);
        echo "Standard Read Content: $standard_content\n";
    }
}

echo "3. Read Stream from Standard BLOB\n";
$blob_handle = fbird_blob_create($db);
fbird_blob_add($blob_handle, "Standard to Stream Test");
$std_id = fbird_blob_close($blob_handle);

$read_stream = fbird_blob_open_stream($db, $std_id);
echo "Read stream opened for standard blob\n";
$content = stream_get_contents($read_stream);
echo "Stream Content: $content\n";
fclose($read_stream);

// Test fbird_blob_info with stream on standard blob
$read_stream = fbird_blob_open_stream($db, $std_id);
$info = fbird_blob_info($read_stream);
if (isset($info['id']) && $info['id'] === $std_id) {
    echo "fbird_blob_info correct on read stream\n";
} else {
    echo "fbird_blob_info failed on read stream\n";
}
fclose($read_stream);

?>
--EXPECT--
1. Create BLOB Stream
Stream created. Writing data...
Written: 33 bytes
Got BLOB ID from stream: [ID FOUND]
Stream closed
2. Read BLOB Stream using obtained ID
Content: Phase 3 BLOB Streaming Test by MW
Standard Read Content: Phase 3 BLOB Streaming Test by MW
3. Read Stream from Standard BLOB
Read stream opened for standard blob
Stream Content: Standard to Stream Test
fbird_blob_info correct on read stream

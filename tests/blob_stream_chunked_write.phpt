--TEST--
Test: BLOB creation from PHP stream with chunked writes
--DESCRIPTION--
This test replicates the doctrine-firebird-driver pattern:
1. Create blob from transaction
2. Read chunks from a PHP data:// stream using fread()
3. Call ibase_blob_add() for each chunk
4. Close the blob

This pattern should work, but has been observed to cause SIGSEGV (exit 139)
in some scenarios with PHP Firebird extension 6.2.0.
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("interbase.inc");

echo "Test: BLOB creation from PHP stream with chunked writes\n";
echo "=========================================================\n";

$db = ibase_connect($test_base);

// Create test table (DDL uses connection-level implicit transaction)
@ibase_query($db, "DROP TABLE test_stream_blob");
ibase_commit($db);  // Commit any pending DDL
ibase_query($db, "CREATE TABLE test_stream_blob (id INTEGER NOT NULL PRIMARY KEY, data BLOB)");
ibase_commit($db);  // Commit table creation

// Test with various data sizes
$sizes = [100, 8192, 32768, 65536];

foreach ($sizes as $size) {
    echo "\nTesting with {$size} bytes:\n";

    $trans = ibase_trans($db);

    // Create test data
    $testData = str_repeat('X', $size);

    // Create a PHP data:// stream (similar to how doctrine-firebird-driver works)
    $stream = fopen('data://text/plain,' . $testData, 'r');
    if (!$stream) {
        echo "Failed to create stream\n";
        continue;
    }

    echo "  - Stream created\n";

    // Create blob
    $blob = ibase_blob_create($trans);
    if (!$blob) {
        echo "  - FAILED: ibase_blob_create returned false\n";
        fclose($stream);
        ibase_rollback($trans);
        continue;
    }

    echo "  - Blob created (type: " . get_resource_type($blob) . ")\n";

    // Write in chunks (same pattern as doctrine-firebird-driver)
    $chunkSize = 8192;
    $totalWritten = 0;
    $chunkCount = 0;

    while (!feof($stream)) {
        $chunk = fread($stream, $chunkSize);
        if ($chunk === false || strlen($chunk) === 0) {
            continue;
        }

        $result = ibase_blob_add($blob, $chunk);
        if ($result === false) {
            echo "  - FAILED: ibase_blob_add returned false at chunk {$chunkCount}\n";
            break;
        }

        $totalWritten += strlen($chunk);
        $chunkCount++;
    }

    echo "  - Written {$totalWritten} bytes in {$chunkCount} chunks\n";

    // Close stream
    fclose($stream);

    // Close blob
    $blobId = ibase_blob_close($blob);
    if ($blobId === false) {
        echo "  - FAILED: ibase_blob_close returned false\n";
        ibase_rollback($trans);
        continue;
    }

    echo "  - Blob closed, ID: " . substr($blobId, 0, 18) . "\n";

    // Insert into table
    $stmt = ibase_prepare($trans, "INSERT INTO test_stream_blob (id, data) VALUES (?, ?)");
    $result = ibase_execute($stmt, $size, $blobId);
    if ($result === false) {
        echo "  - FAILED: ibase_execute returned false\n";
        ibase_rollback($trans);
        continue;
    }

    ibase_commit($trans);
    echo "  - SUCCESS: Data inserted and committed\n";

    // Verify
    $trans = ibase_trans($db);
    $result = ibase_query($trans, "SELECT data FROM test_stream_blob WHERE id = {$size}");
    $row = ibase_fetch_assoc($result, IBASE_TEXT);

    if ($row && strlen($row['DATA']) === $size) {
        echo "  - VERIFIED: Retrieved " . strlen($row['DATA']) . " bytes\n";
    } else {
        echo "  - VERIFY FAILED\n";
    }

    ibase_commit($trans);
}

// Cleanup - commit any pending DDL and suppress warnings
@ibase_query($db, "DROP TABLE test_stream_blob");
@ibase_commit($db);  // Commit DROP TABLE DDL (may warn if table was already dropped)
@ibase_close($db);

echo "\nDone!\n";
?>
--EXPECTF--
Test: BLOB creation from PHP stream with chunked writes
=========================================================

Testing with 100 bytes:
  - Stream created
  - Blob created (type: Firebird/InterBase blob)
  - Written 100 bytes in 1 chunks
  - Blob closed, ID: 0x%s
  - SUCCESS: Data inserted and committed
  - VERIFIED: Retrieved 100 bytes

Testing with 8192 bytes:
  - Stream created
  - Blob created (type: Firebird/InterBase blob)
  - Written 8192 bytes in 1 chunks
  - Blob closed, ID: 0x%s
  - SUCCESS: Data inserted and committed
  - VERIFIED: Retrieved 8192 bytes

Testing with 32768 bytes:
  - Stream created
  - Blob created (type: Firebird/InterBase blob)
  - Written 32768 bytes in 4 chunks
  - Blob closed, ID: 0x%s
  - SUCCESS: Data inserted and committed
  - VERIFIED: Retrieved 32768 bytes

Testing with 65536 bytes:
  - Stream created
  - Blob created (type: Firebird/InterBase blob)
  - Written 65536 bytes in 8 chunks
  - Blob closed, ID: 0x%s
  - SUCCESS: Data inserted and committed
  - VERIFIED: Retrieved 65536 bytes

Done!

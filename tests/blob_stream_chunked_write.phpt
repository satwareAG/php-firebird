--TEST--
Test: BLOB creation from PHP stream with chunked writes
--DESCRIPTION--
This test replicates the doctrine-firebird-driver pattern:
1. Create blob from transaction
2. Read chunks from a PHP data:// stream using fread()
3. Call fbird_blob_add() for each chunk
4. Close the blob

Tests chunked BLOB writes with various sizes (100B, 8KB, 32KB, 64KB).
Historically triggered SIGSEGV/heap corruption (Issue #10).
Fixed in 6.2.1 via zend_list_close() replacing zend_list_delete().
Kept as a regression test.
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php
require("firebird.inc");

echo "Test: BLOB creation from PHP stream with chunked writes\n";
echo "=========================================================\n";

$db = fbird_connect($test_base);

// Create test table (DDL uses connection-level implicit transaction)
@fbird_query($db, "DROP TABLE test_stream_blob");
fbird_commit($db);  // Commit any pending DDL
fbird_query($db, "CREATE TABLE test_stream_blob (id INTEGER NOT NULL PRIMARY KEY, data BLOB)");
fbird_commit($db);  // Commit table creation

// Test with various data sizes
$sizes = [100, 8192, 32768, 65536];

foreach ($sizes as $size) {
    echo "\nTesting with {$size} bytes:\n";

    $trans = fbird_trans($db);

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
    $blob = fbird_blob_create($trans);
    if (!$blob) {
        echo "  - FAILED: fbird_blob_create returned false\n";
        fclose($stream);
        fbird_rollback($trans);
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

        $result = fbird_blob_add($blob, $chunk);
        if ($result === false) {
            echo "  - FAILED: fbird_blob_add returned false at chunk {$chunkCount}\n";
            break;
        }

        $totalWritten += strlen($chunk);
        $chunkCount++;
    }

    echo "  - Written {$totalWritten} bytes in {$chunkCount} chunks\n";

    // Close stream
    fclose($stream);

    // Close blob
    $blobId = fbird_blob_close($blob);
    if ($blobId === false) {
        echo "  - FAILED: fbird_blob_close returned false\n";
        fbird_rollback($trans);
        continue;
    }

    echo "  - Blob closed, ID: " . substr($blobId, 0, 18) . "\n";

    // Insert into table
    $stmt = fbird_prepare($trans, "INSERT INTO test_stream_blob (id, data) VALUES (?, ?)");
    $result = fbird_execute($stmt, $size, $blobId);
    if ($result === false) {
        echo "  - FAILED: fbird_execute returned false\n";
        fbird_rollback($trans);
        continue;
    }

    fbird_commit($trans);
    echo "  - SUCCESS: Data inserted and committed\n";

    // Verify
    $trans = fbird_trans($db);
    $result = fbird_query($trans, "SELECT data FROM test_stream_blob WHERE id = {$size}");
    $row = fbird_fetch_assoc($result, FBIRD_TEXT);

    if ($row && strlen($row['DATA']) === $size) {
        echo "  - VERIFIED: Retrieved " . strlen($row['DATA']) . " bytes\n";
    } else {
        echo "  - VERIFY FAILED\n";
    }

    fbird_commit($trans);
}

// Cleanup - commit any pending DDL and suppress warnings
echo "\n  - Cleanup: starting DROP...\n";
@fbird_query($db, "DROP TABLE test_stream_blob");
echo "  - Cleanup: DROP executed\n";
@fbird_commit($db);  // Commit DROP TABLE DDL (may warn if table was already dropped)
echo "  - Cleanup: committed\n";
@fbird_close($db);
echo "  - Cleanup: closed\n";

echo "\nDone!\n";
?>
--EXPECTF--
Test: BLOB creation from PHP stream with chunked writes
=========================================================

Testing with 100 bytes:
  - Stream created
  - Blob created (type: Firebird blob)
  - Written 100 bytes in 1 chunks
  - Blob closed, ID: 0x%s
  - SUCCESS: Data inserted and committed
  - VERIFIED: Retrieved 100 bytes

Testing with 8192 bytes:
  - Stream created
  - Blob created (type: Firebird blob)
  - Written 8192 bytes in 1 chunks
  - Blob closed, ID: 0x%s
  - SUCCESS: Data inserted and committed
  - VERIFIED: Retrieved 8192 bytes

Testing with 32768 bytes:
  - Stream created
  - Blob created (type: Firebird blob)
  - Written 32768 bytes in 4 chunks
  - Blob closed, ID: 0x%s
  - SUCCESS: Data inserted and committed
  - VERIFIED: Retrieved 32768 bytes

Testing with 65536 bytes:
  - Stream created
  - Blob created (type: Firebird blob)
  - Written 65536 bytes in 8 chunks
  - Blob closed, ID: 0x%s
  - SUCCESS: Data inserted and committed
  - VERIFIED: Retrieved 65536 bytes

  - Cleanup: starting DROP...
  - Cleanup: DROP executed
  - Cleanup: committed
  - Cleanup: closed

Done!

<?php
/**
 * Blob Operations Sanitizer Test
 * 
 * Tests blob creation, writing, and reading to detect leaks in blob handling.
 */

require_once __DIR__ . '/../firebird.inc';

echo "ASan Blob Test: Starting...\n";

$dbh = fbird_connect($test_base, $user, $password);
if (!$dbh) die("Connect failed");

// Create a test table if not exists
// Use a separate transaction for DDL to ensure it's committed
$trans = fbird_trans($dbh);
@fbird_query($trans, "DROP TABLE TEST_BLOB_SAN");
fbird_commit($trans);

$trans = fbird_trans($dbh);
if (!fbird_query($trans, "CREATE TABLE TEST_BLOB_SAN (ID INT, DATA BLOB)")) {
    echo "Create table failed: " . fbird_errmsg() . "\n";
    fbird_rollback($trans);
    exit(1);
}
fbird_commit($trans);

// Insert Blob
$blob_handle = fbird_blob_create($dbh);
$data = str_repeat("A", 10000);
fbird_blob_add($blob_handle, $data);
$blob_id = fbird_blob_close($blob_handle);

if (!fbird_query($dbh, "INSERT INTO TEST_BLOB_SAN (ID, DATA) VALUES (1, ?)", $blob_id)) {
    echo "Insert failed: " . fbird_errmsg() . "\n";
    exit(1);
}
echo "Blob inserted.\n";

// Read Blob
$res = fbird_query($dbh, "SELECT DATA FROM TEST_BLOB_SAN WHERE ID = 1");
if ($res) {
    $row = fbird_fetch_object($res);
    if ($row) {
        $blob_info = fbird_blob_info($dbh, $row->DATA);
        $content = "";
        $h = fbird_blob_open($dbh, $row->DATA);
        while ($chunk = fbird_blob_get($h, 1024)) {
            $content .= $chunk;
        }
        fbird_blob_close($h);
        
        if (strlen($content) !== 10000) {
            echo "Blob length mismatch!\n";
        } else {
            echo "Blob read verified.\n";
        }
    }
    fbird_free_result($res);
} else {
    echo "Select failed: " . fbird_errmsg() . "\n";
}

// Cleanup
$trans = fbird_trans($dbh);
fbird_query($trans, "DROP TABLE TEST_BLOB_SAN");
fbird_commit($trans);

fbird_close($dbh);
echo "ASan Blob Test: Completed.\n";

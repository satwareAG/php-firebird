<?php
/**
 * Blob Operations Sanitizer Test
 * 
 * Tests blob creation, writing, and reading to detect leaks in blob handling.
 */

require_once __DIR__ . '/../config.inc';

echo "ASan Blob Test: Starting...\n";

$dbh = fbird_connect($test_base, $user, $password);
if (!$dbh) die("Connect failed");

// Create a test table if not exists
@fbird_query($dbh, "DROP TABLE TEST_BLOB_SAN");
fbird_query($dbh, "CREATE TABLE TEST_BLOB_SAN (ID INT, DATA BLOB)");

// Insert Blob
$blob_handle = fbird_blob_create($dbh);
$data = str_repeat("A", 10000);
fbird_blob_add($blob_handle, $data);
$blob_id = fbird_blob_close($blob_handle);

fbird_query($dbh, "INSERT INTO TEST_BLOB_SAN (ID, DATA) VALUES (1, ?)", $blob_id);
echo "Blob inserted.\n";

// Read Blob
$res = fbird_query($dbh, "SELECT DATA FROM TEST_BLOB_SAN WHERE ID = 1");
$row = fbird_fetch_object($res);
$blob_info = fbird_blob_info($dbh, $row->DATA);
$content = "";
$h = fbird_blob_open($dbh, $row->DATA);
while ($chunk = fbird_blob_get($h, 1024)) {
    $content .= $chunk;
}
fbird_blob_close($h);
fbird_free_result($res);

if (strlen($content) !== 10000) {
    echo "Blob length mismatch!\n";
} else {
    echo "Blob read verified.\n";
}

// Cleanup
fbird_query($dbh, "DROP TABLE TEST_BLOB_SAN");
fbird_close($dbh);
echo "ASan Blob Test: Completed.\n";

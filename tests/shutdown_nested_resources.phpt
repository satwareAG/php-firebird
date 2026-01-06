--TEST--
Shutdown nested resource cleanup order - blobs, queries, transactions
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for shutdown segfault prevention with deeply nested resources:
 * Tests correct destructor ordering when multiple resource types are in use.
 *
 * PHP's resource cleanup order during shutdown is not guaranteed to match
 * the logical dependency order. This test creates a hierarchy of resources:
 * - Connection -> Transaction -> Query -> Result
 * - Connection -> Transaction -> Blob
 *
 * The destructors must handle any cleanup order without crashing.
 *
 * Expected behavior: Clean exit (exit code 0) with "Test completed" output.
 * Failure mode: SIGSEGV crash (exit code 139) during shutdown.
 *
 * Related: Issue #56 (fork safety), blob destructor guards
 */

// Create connection
$db = fbird_connect($test_base);
if (!$db) {
    die("Connection failed: " . fbird_errmsg());
}

// Create explicit transaction
$trans = fbird_trans($db);
if (!$trans) {
    die("Transaction failed: " . fbird_errmsg());
}

// Prepare and execute a query
$query = fbird_prepare($db, "SELECT RDB\$RELATION_NAME FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'TEST1'");
if (!$query) {
    die("Prepare failed: " . fbird_errmsg());
}

$result = fbird_execute($query);
if (!$result) {
    die("Execute failed: " . fbird_errmsg());
}

// Create a blob (input blob for writing)
$blob = fbird_blob_create($db);
if (!$blob) {
    die("Blob create failed: " . fbird_errmsg());
}

// Write some data to the blob
$test_data = "Test blob data for shutdown safety test";
if (!fbird_blob_add($blob, $test_data)) {
    die("Blob add failed: " . fbird_errmsg());
}

// Close the blob to get its ID (but don't free the resource fully)
$blob_id = fbird_blob_close($blob);
if (!$blob_id) {
    die("Blob close failed: " . fbird_errmsg());
}

// Open the blob for reading (creates another blob resource)
$blob_read = fbird_blob_open($db, $blob_id);
if (!$blob_read) {
    die("Blob open failed: " . fbird_errmsg());
}

// Read the blob data
$read_data = fbird_blob_get($blob_read, 1000);
if ($read_data === false) {
    die("Blob get failed: " . fbird_errmsg());
}

// Verify blob data
if ($read_data !== $test_data) {
    die("Blob data mismatch: expected '$test_data', got '$read_data'");
}

// Fetch a row from the query result to verify it's active
$row = fbird_fetch_row($result);
// May return false if TEST1 doesn't exist in RDB$RELATIONS, that's OK

// IMPORTANT: Do NOT call any cleanup functions here!
// Resources left open: $db, $trans, $query, $result, $blob_read
// The purpose is to verify that nested resource destructors handle
// shutdown correctly regardless of cleanup order.

echo "Test completed\n";
?>
--EXPECT--
Test completed

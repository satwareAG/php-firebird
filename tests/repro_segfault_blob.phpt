--TEST--
Bug: Segfault on resource cleanup with BLOBs
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$x = fbird_connect($test_base);
$trans = fbird_trans($x);

// 1. INSERT with BLOB
$sql = "CREATE TABLE test_blobs (id INT, data BLOB)";
@fbird_query($trans, $sql);
fbird_commit($trans);

$trans = fbird_trans($x);
$blob_id = fbird_blob_create($trans);
fbird_blob_add($blob_id, "Some data");
$blob_id_str = fbird_blob_close($blob_id);

$query = fbird_prepare($trans, "INSERT INTO test_blobs (id, data) VALUES (?, ?)");
fbird_execute($query, 1, $blob_id_str);
fbird_commit($trans);

// 2. SELECT BLOB
$trans = fbird_trans($x);
$query = fbird_prepare($trans, "SELECT data FROM test_blobs WHERE id = 1");
$res = fbird_execute($query);

// 3. Fetch BLOB
$row = fbird_fetch_row($res);
$blob_handle = fbird_blob_open($trans, $row[0]);
// Note: Using $trans as link identifier for blob

$data = fbird_blob_get($blob_handle, 100);
// echo "Data: $data\n";

// 4. Commit (Invalidates cursors/transaction context)
fbird_commit($trans);

// 5. Explicit frees or Shutdown cleanup
// The report says: "fbird_free_result vs fbird_free_query race condition during PHP shutdown."
// We can trigger it by NOT freeing explicitly, letting PHP shutdown handle it.
// Or trying to free explicitly after commit.

// Attempt explicit free of invalid resource:
// fbird_free_result($res); // Should handle graceful failure if transaction closed?

echo "Done\n";
?>
--EXPECTF--
Done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

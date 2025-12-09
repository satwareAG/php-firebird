--TEST--
Bug: Segfault on resource cleanup with BLOBs
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$x = ibase_connect($test_base);
$trans = ibase_trans($x);

// 1. INSERT with BLOB
$sql = "CREATE TABLE test_blobs (id INT, data BLOB)";
@ibase_query($trans, $sql);
ibase_commit($trans);

$trans = ibase_trans($x);
$blob_id = ibase_blob_create($trans);
ibase_blob_add($blob_id, "Some data");
$blob_id_str = ibase_blob_close($blob_id);

$query = ibase_prepare($trans, "INSERT INTO test_blobs (id, data) VALUES (?, ?)");
ibase_execute($query, 1, $blob_id_str);
ibase_commit($trans);

// 2. SELECT BLOB
$trans = ibase_trans($x);
$query = ibase_prepare($trans, "SELECT data FROM test_blobs WHERE id = 1");
$res = ibase_execute($query);

// 3. Fetch BLOB
$row = ibase_fetch_row($res);
$blob_handle = ibase_blob_open($trans, $row[0]);
// Note: Using $trans as link identifier for blob

$data = ibase_blob_get($blob_handle, 100);
// echo "Data: $data\n";

// 4. Commit (Invalidates cursors/transaction context)
ibase_commit($trans);

// 5. Explicit frees or Shutdown cleanup
// The report says: "fbird_free_result vs fbird_free_query race condition during PHP shutdown."
// We can trigger it by NOT freeing explicitly, letting PHP shutdown handle it.
// Or trying to free explicitly after commit.

// Attempt explicit free of invalid resource:
// ibase_free_result($res); // Should handle graceful failure if transaction closed?

echo "Done\n";
?>
--CLEAN--
<?php
require("firebird.inc");
$x = ibase_connect($test_base);
@ibase_query($x, "DROP TABLE test_blobs");
ibase_close($x);
?>
--EXPECTF--
Done

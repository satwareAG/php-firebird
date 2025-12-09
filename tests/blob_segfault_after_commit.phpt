--TEST--
Bug: SIGSEGV when using blob_add after transaction commit/rollback
--DESCRIPTION--
This test demonstrates a crash (SIGSEGV) when fbird_blob_add() is called
with a blob handle that was created on a transaction that has been
committed or rolled back. The blob handle becomes invalid, but the extension
doesn't check for this in _php_fbird_blob_add() before calling isc_put_segment().

Unlike _php_fbird_free_blob() which checks "ib_blob->bl_handle.ptr != 0",
_php_fbird_blob_add() has no such safety check.
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Connecting...\n";
$db = fbird_connect($test_base);

echo "Starting transaction...\n";
$trans = fbird_trans($db);

echo "Creating blob...\n";
$blob = fbird_blob_create($trans);

// Blob is created, but now we commit the transaction
// This should invalidate the blob handle
echo "Committing transaction (invalidates blob)...\n";
fbird_commit($trans);

echo "Attempting blob_add on invalid handle...\n";
// This should NOT crash - should return false or throw an error
// But currently it causes SIGSEGV
$result = @fbird_blob_add($blob, "test data");
if ($result === false) {
    echo "blob_add correctly returned false for invalid handle\n";
} else {
    echo "blob_add unexpectedly succeeded\n";
}

echo "Done\n";
fbird_close($db);
?>
--EXPECTF--
Connecting...
Starting transaction...
Creating blob...
Committing transaction (invalidates blob)...
Attempting blob_add on invalid handle...
blob_add correctly returned false for invalid handle
Done

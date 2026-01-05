--TEST--
UAF detection: Using blob after fbird_blob_close()
--DESCRIPTION--
Verify that attempting to use a blob resource after closing it
produces a proper error rather than undefined behavior or memory corruption.
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base);
if (!$conn) {
    die("Cannot connect: " . fbird_errmsg());
}

// Create a blob for writing
$blob = fbird_blob_create($conn);
if (!$blob) {
    die("Cannot create blob: " . fbird_errmsg());
}

echo "Blob created successfully\n";

// Write some data
$data = "Test blob data for UAF detection";
$written = fbird_blob_add($blob, $data);
echo "Data written to blob\n";

// Close and get blob ID
$blob_id = fbird_blob_close($blob);
echo "Blob closed, id: " . (is_string($blob_id) ? "valid" : "invalid") . "\n";

// Attempt operations on closed blob (should error, not crash)
echo "Attempting to use closed blob...\n";

// Try to add data to closed blob
$add_result = @fbird_blob_add($blob, "more data");
if ($add_result === false) {
    echo "Add to closed blob failed as expected\n";
} else {
    echo "ERROR: Add should have failed!\n";
}

// Try to close again
$close2 = @fbird_blob_close($blob);
if ($close2 === false) {
    echo "Re-close failed as expected\n";
} else {
    echo "ERROR: Re-close should have failed!\n";
}

// Try to cancel closed blob
$cancel = @fbird_blob_cancel($blob);
if ($cancel === false) {
    echo "Cancel closed blob failed as expected\n";
} else {
    echo "ERROR: Cancel should have failed!\n";
}

fbird_close($conn);
echo "Test completed without crash\n";
?>
--EXPECT--
Blob created successfully
Data written to blob
Blob closed, id: valid
Attempting to use closed blob...
Add to closed blob failed as expected
Re-close failed as expected
Cancel closed blob failed as expected
Test completed without crash

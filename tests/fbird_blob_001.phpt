--TEST--
fbird_blob_create() and fbird_blob_cancel() handle invalidation
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

// Test basic create and cancel
$blob = fbird_blob_create($db);
var_dump(is_resource($blob) || $blob instanceof \Firebird\Blob);
var_dump(fbird_blob_add($blob, "test data"));
var_dump(fbird_blob_cancel($blob));

// Verify handle is unusable
// In PHP 8+, cancelled/closed resources typically return false for is_resource()
$is_valid = is_resource($blob) && get_resource_type($blob) !== 'Unknown';
var_dump($is_valid);

// Attempting to add data to a cancelled blob should fail
// Use @ to suppress Warning in PHP 8.1 (returns false)
// Use try-catch to handle Error in PHP 8.3 (throws)
try {
    $res = @fbird_blob_add($blob, "more data");
    if ($res === false) {
        echo "Add failed\n";
    } else {
        echo "Add succeeded unexpectedly\n";
    }
} catch (\Throwable $e) {
    echo "Add failed\n"; // . $e->getMessage();
}

// Attempting to close a cancelled blob
try {
    $res = @fbird_blob_close($blob);
    if ($res === false) {
        echo "Close failed\n";
    } else {
        echo "Close succeeded unexpectedly\n";
    }
} catch (\Throwable $e) {
    echo "Close failed\n";
}

// Cleanup
var_dump(fbird_close($db));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(false)
Add failed
Close failed
bool(true)

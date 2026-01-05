--TEST--
UAF detection: Using query after fbird_free_query()
--DESCRIPTION--
Verify that attempting to use a query resource after freeing it
produces a proper error rather than undefined behavior or memory corruption.
This tests the magic number validation infrastructure.
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

// Prepare a simple query
$query = fbird_prepare($conn, 'SELECT 1 AS result FROM RDB$DATABASE');
if (!$query) {
    die("Cannot prepare: " . fbird_errmsg());
}

echo "Query prepared successfully\n";

// Execute once (should work)
$result = fbird_execute($query);
if ($result) {
    $row = fbird_fetch_assoc($result);
    echo "First execute: result = " . $row['RESULT'] . "\n";
    fbird_free_result($result);
}

// Free the query
$freed = fbird_free_query($query);
echo "Query freed: " . ($freed ? "true" : "false") . "\n";

// Attempt to use the freed query (should error, not crash)
echo "Attempting to execute freed query...\n";
try {
    $result2 = @fbird_execute($query);
    if ($result2 === false) {
        echo "Execute failed as expected (returned false)\n";
    } else {
        echo "ERROR: Execute should have failed!\n";
    }
} catch (Throwable $e) {
    echo "Caught exception: " . get_class($e) . "\n";
}

// Attempt to get num_params on freed query
try {
    $params = @fbird_num_params($query);
    if ($params === false) {
        echo "num_params failed as expected (returned false)\n";
    } else {
        echo "ERROR: num_params should have failed!\n";
    }
} catch (Throwable $e) {
    echo "Caught exception: " . get_class($e) . "\n";
}

fbird_close($conn);
echo "Test completed without crash\n";
?>
--EXPECT--
Query prepared successfully
First execute: result = 1
Query freed: true
Attempting to execute freed query...
Execute failed as expected (returned false)
num_params failed as expected (returned false)
Test completed without crash

--TEST--
UAF detection: Using result after parent query/connection freed
--DESCRIPTION--
Verify that using result resources after their parent query or connection
is freed produces proper errors rather than crashes.
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

// Create a query and get result
$query = fbird_query($conn, 'SELECT 1 AS val, 2 AS val2, 3 AS val3 FROM RDB$DATABASE');
if (!$query) {
    die("Cannot execute query: " . fbird_errmsg());
}

echo "Query executed successfully\n";

// Fetch one row - result is now active
$row = fbird_fetch_assoc($query);
echo "First fetch: val = " . $row['VAL'] . "\n";

// Free the query while result is still theoretically usable
$freed = fbird_free_result($query);
echo "Query freed: " . ($freed ? "true" : "false") . "\n";

// Try to fetch from freed query (should error, not crash)
echo "Attempting to use freed result...\n";

$row2 = @fbird_fetch_assoc($query);
if ($row2 === false) {
    echo "Fetch from freed query returned false as expected\n";
} else {
    echo "ERROR: Fetch should have failed but returned data\n";
}

// Try other result operations
$num = @fbird_num_fields($query);
if ($num === false) {
    echo "num_fields on freed query returned false as expected\n";
} else {
    // Some implementations may cache metadata
    echo "num_fields returned: $num (may be cached)\n";
}

// Double-free test - throws TypeError
try {
    $freed2 = fbird_free_result($query);
    echo "Double-free returned: " . ($freed2 ? "true" : "false") . "\n";
} catch (TypeError $e) {
    echo "Double-free threw TypeError as expected\n";
}

fbird_close($conn);
echo "Test completed without crash\n";
?>
--EXPECT--
Query executed successfully
First fetch: val = 1
Query freed: true
Attempting to use freed result...
Fetch from freed query returned false as expected
num_fields on freed query returned false as expected
Double-free threw TypeError as expected
Test completed without crash

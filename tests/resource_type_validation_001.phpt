--TEST--
Resource type validation: TypeError exceptions for wrong resource types
--DESCRIPTION--
Verifies that passing wrong resource types (e.g., connection to a function expecting
query, or transaction to a function expecting connection) throws TypeError with
clear error messages when exception mode is enabled.
--SKIPIF--
<?php
include("skipif.inc");
?>
--FILE--
<?php
require("firebird.inc");

// Enable exception mode for TypeError throwing
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);

// Connect to get a valid connection resource
$conn = fbird_connect($test_base);
if (!$conn) {
    die("Failed to connect");
}

// Start a transaction to get a valid transaction resource
$trans = fbird_trans($conn);
if (!$trans) {
    die("Failed to start transaction");
}

// Prepare a query to get a valid query resource
$query = fbird_prepare($conn, "SELECT 1 FROM RDB\$DATABASE");
if (!$query) {
    die("Failed to prepare query");
}

echo "=== Test 1: fbird_execute() with connection instead of query ===\n";
try {
    // Pass connection resource where query resource is expected
    fbird_execute($conn);
    echo "ERROR: Expected TypeError was not thrown\n";
} catch (TypeError $e) {
    echo "TypeError caught: " . $e->getMessage() . "\n";
}

echo "\n=== Test 2: fbird_fetch_assoc() with connection instead of query ===\n";
try {
    // Pass connection resource where query resource is expected
    fbird_fetch_assoc($conn);
    echo "ERROR: Expected TypeError was not thrown\n";
} catch (TypeError $e) {
    echo "TypeError caught: " . $e->getMessage() . "\n";
}

echo "\n=== Test 3: fbird_fetch_row() with transaction instead of query ===\n";
try {
    // Pass transaction resource where query resource is expected
    fbird_fetch_row($trans);
    echo "ERROR: Expected TypeError was not thrown\n";
} catch (TypeError $e) {
    echo "TypeError caught: " . $e->getMessage() . "\n";
}

echo "\n=== Test 4: fbird_num_fields() with transaction instead of query ===\n";
try {
    // Pass transaction resource where query resource is expected
    fbird_num_fields($trans);
    echo "ERROR: Expected TypeError was not thrown\n";
} catch (TypeError $e) {
    echo "TypeError caught: " . $e->getMessage() . "\n";
}

echo "\n=== Test 5: fbird_field_info() with connection instead of query ===\n";
try {
    // Pass connection resource where query resource is expected
    fbird_field_info($conn, 0);
    echo "ERROR: Expected TypeError was not thrown\n";
} catch (TypeError $e) {
    echo "TypeError caught: " . $e->getMessage() . "\n";
}

echo "\n=== Test 6: fbird_num_params() with connection instead of query ===\n";
try {
    // Pass connection resource where query resource is expected
    fbird_num_params($conn);
    echo "ERROR: Expected TypeError was not thrown\n";
} catch (TypeError $e) {
    echo "TypeError caught: " . $e->getMessage() . "\n";
}

echo "\n=== Test 7: fbird_param_info() with transaction instead of query ===\n";
try {
    // Pass transaction resource where query resource is expected
    fbird_param_info($trans, 0);
    echo "ERROR: Expected TypeError was not thrown\n";
} catch (TypeError $e) {
    echo "TypeError caught: " . $e->getMessage() . "\n";
}

echo "\n=== Test 8: fbird_name_result() with connection instead of query ===\n";
try {
    // Pass connection resource where query resource is expected
    fbird_name_result($conn, "test_cursor");
    echo "ERROR: Expected TypeError was not thrown\n";
} catch (TypeError $e) {
    echo "TypeError caught: " . $e->getMessage() . "\n";
}

// Cleanup
fbird_free_query($query);
fbird_rollback($trans);
fbird_close($conn);

echo "\n=== All tests completed ===\n";
?>
--EXPECTF--
=== Test 1: fbird_execute() with connection instead of query ===
TypeError caught: fbird_execute(): Argument #1 ($query) must be a Firebird query resource or Firebird\ResultSet, object given

=== Test 2: fbird_fetch_assoc() with connection instead of query ===
TypeError caught: fbird_fetch_assoc(): Argument #1 ($result) must be a Firebird query/result resource, object given

=== Test 3: fbird_fetch_row() with transaction instead of query ===
TypeError caught: fbird_fetch_row(): Argument #1 ($result) must be a Firebird query/result resource, object given

=== Test 4: fbird_num_fields() with transaction instead of query ===
TypeError caught: fbird_num_fields(): Argument #1 ($query_result) must be a Firebird query/result resource, object given

=== Test 5: fbird_field_info() with connection instead of query ===
TypeError caught: fbird_field_info(): Argument #1 ($query_result) must be a Firebird query/result resource, object given

=== Test 6: fbird_num_params() with connection instead of query ===
TypeError caught: fbird_num_params(): Argument #1 ($query) must be a Firebird query/result resource, object given

=== Test 7: fbird_param_info() with transaction instead of query ===
TypeError caught: fbird_param_info(): Argument #1 ($query) must be a Firebird query/result resource, object given

=== Test 8: fbird_name_result() with connection instead of query ===
TypeError caught: fbird_name_result(): Argument #1 ($result) must be a Firebird query/result resource, object given

=== All tests completed ===

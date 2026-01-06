--TEST--
UAF: Persistent connection cache integrity after close
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/**
 * Tests that persistent connection cache maintains integrity after operations.
 * 
 * Scenario: Verifies that persistent connections can be safely reused
 * and that the cache doesn't return freed memory addresses.
 * 
 * Expected Behavior:
 * - First pconnect creates a persistent connection
 * - Closing the resource marks it for reuse
 * - Second pconnect returns the cached connection (or new if properly cleaned)
 * - Operations on reused connection should work without UAF
 */
require("firebird.inc");

echo "=== UAF: Persistent Connection Cache Integrity ===\n\n";

// Test 1: Basic persistent connection reuse
echo "Test 1: Basic pconnect reuse\n";
$conn1 = fbird_pconnect($test_base, $user, $password);
if (!$conn1) {
    die("Failed to create first persistent connection: " . fbird_errmsg());
}
echo "Created first persistent connection\n";

// Execute a simple query to verify connection works
$result1 = fbird_query($conn1, "SELECT 1 FROM RDB\$DATABASE");
if ($result1) {
    $row = fbird_fetch_row($result1);
    echo "First connection query result: " . $row[0] . "\n";
    fbird_free_result($result1);
}

// Close the persistent connection (marks it for potential reuse)
$close_result = fbird_close($conn1);
echo "Close result: " . ($close_result ? "true" : "false") . "\n";

// Try to get another persistent connection (may reuse or create new)
$conn2 = fbird_pconnect($test_base, $user, $password);
if (!$conn2) {
    die("Failed to create second persistent connection: " . fbird_errmsg());
}
echo "Created second persistent connection\n";

// Execute query on second connection - should not crash or return garbage
$result2 = fbird_query($conn2, "SELECT 2 FROM RDB\$DATABASE");
if ($result2) {
    $row = fbird_fetch_row($result2);
    echo "Second connection query result: " . $row[0] . "\n";
    fbird_free_result($result2);
} else {
    echo "ERROR: Query on second connection failed: " . fbird_errmsg() . "\n";
}

echo "\n";

// Test 2: Multiple sequential pconnect/close cycles
echo "Test 2: Multiple pconnect/close cycles\n";
for ($i = 1; $i <= 3; $i++) {
    $conn = fbird_pconnect($test_base, $user, $password);
    if (!$conn) {
        echo "Cycle $i: Failed to connect: " . fbird_errmsg() . "\n";
        continue;
    }
    
    $result = fbird_query($conn, "SELECT $i FROM RDB\$DATABASE");
    if ($result) {
        $row = fbird_fetch_row($result);
        echo "Cycle $i: Query result = " . $row[0] . "\n";
        fbird_free_result($result);
    }
    
    fbird_close($conn);
}

echo "\n";

// Test 3: Verify using closed pconnect handle fails safely
echo "Test 3: Using closed pconnect handle\n";
$conn3 = fbird_pconnect($test_base, $user, $password);
if (!$conn3) {
    die("Failed to create test connection: " . fbird_errmsg());
}
fbird_close($conn3);

// This should fail safely with TypeError, not crash
try {
    $result = @fbird_query($conn3, "SELECT 1 FROM RDB\$DATABASE");
    echo "Query on closed handle: " . ($result === false ? "false (correct)" : "unexpected success") . "\n";
} catch (TypeError $e) {
    echo "Query on closed handle threw TypeError (correct)\n";
}

echo "\n=== All persistent connection cache tests completed ===\n";

// Clean up - close $conn2 which is still open
fbird_close($conn2);
?>
--EXPECTF--
=== UAF: Persistent Connection Cache Integrity ===

Test 1: Basic pconnect reuse
Created first persistent connection
First connection query result: 1
Close result: true
Created second persistent connection
Second connection query result: 2

Test 2: Multiple pconnect/close cycles
Cycle 1: Query result = 1
Cycle 2: Query result = 2
Cycle 3: Query result = 3

Test 3: Using closed pconnect handle
Query on closed handle%s

=== All persistent connection cache tests completed ===

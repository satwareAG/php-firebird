--TEST--
UAF detection: Using transaction after fbird_commit()
--DESCRIPTION--
Verify that attempting to use a transaction resource after committing it
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

// Start explicit transaction
$trans = fbird_trans(FBIRD_DEFAULT, $conn);
if (!$trans) {
    die("Cannot start transaction: " . fbird_errmsg());
}

echo "Transaction started successfully\n";

// Use transaction for a query
$query = fbird_query($trans, 'SELECT 1 AS result FROM RDB$DATABASE');
if ($query) {
    $row = fbird_fetch_assoc($query);
    echo "Query in transaction: result = " . $row['RESULT'] . "\n";
    fbird_free_result($query);
}

// Commit the transaction
$committed = fbird_commit($trans);
echo "Transaction committed: " . ($committed ? "true" : "false") . "\n";

// Attempt to use the committed transaction (should error, not crash)
echo "Attempting to use committed transaction...\n";

// Try to execute a query with committed transaction
$result2 = @fbird_query($trans, 'SELECT 1 FROM RDB$DATABASE');
if ($result2 === false) {
    echo "Query with committed trans failed as expected\n";
} else {
    echo "ERROR: Query should have failed!\n";
    fbird_free_result($result2);
}

// Try to commit again
$recommit = @fbird_commit($trans);
if ($recommit === false) {
    echo "Re-commit failed as expected\n";
} else {
    echo "ERROR: Re-commit should have failed!\n";
}

// Try to rollback committed transaction
$rollback = @fbird_rollback($trans);
if ($rollback === false) {
    echo "Rollback of committed trans failed as expected\n";
} else {
    echo "ERROR: Rollback should have failed!\n";
}

fbird_close($conn);
echo "Test completed without crash\n";
?>
--EXPECT--
Transaction started successfully
Query in transaction: result = 1
Transaction committed: true
Attempting to use committed transaction...
Query with committed trans failed as expected
Re-commit failed as expected
Rollback of committed trans failed as expected
Test completed without crash

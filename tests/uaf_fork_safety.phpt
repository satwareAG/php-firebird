--TEST--
UAF: Fork safety - child process doesn't corrupt parent resources
--EXTENSIONS--
firebird
pcntl
--XFAIL--
PID tracking not yet implemented - child destructor closes parent socket (Issue #XX)
--SKIPIF--
<?php
include("skipif.inc");
if (!function_exists('pcntl_fork')) {
    die('skip - pcntl extension not available');
}
?>
--FILE--
<?php
/**
 * Tests that forked child processes don't corrupt parent's Firebird resources.
 * 
 * Scenario: When pcntl_fork() is called, child inherits file descriptors
 * and resource handles. When child exits, destructors run. If not properly
 * handled, child's cleanup can corrupt parent's still-active connections.
 * 
 * Expected Behavior:
 * - Parent creates connection and transaction
 * - Fork creates child process
 * - Child exits (destructors run)
 * - Parent continues using connection without crash/corruption
 * 
 * The extension uses PID tracking (IBG(init_pid) and per-resource created_pid)
 * to detect forked processes and skip cleanup.
 */
require("firebird.inc");

echo "=== UAF: Fork Safety Test ===\n\n";

// Create connection and transaction in parent
$conn = fbird_connect($test_base, $user, $password);
if (!$conn) {
    die("Failed to connect: " . fbird_errmsg());
}
echo "Parent: Created connection (PID: " . getmypid() . ")\n";

// Create a transaction
$trans = fbird_trans(FBIRD_READ, $conn);
if (!$trans) {
    die("Failed to start transaction: " . fbird_errmsg());
}
echo "Parent: Created transaction\n";

// Fork child process
$pid = pcntl_fork();

if ($pid === -1) {
    die("Failed to fork");
} elseif ($pid === 0) {
    // Child process
    echo "Child: Started (PID: " . getmypid() . ")\n";
    
    // Child inherited $conn and $trans but should NOT use them
    // (they're still pointing to parent's resources)
    
    // Try a simple operation that doesn't require the connection
    echo "Child: Exiting normally\n";
    
    // When child exits, destructors will run for inherited resources
    // The extension should detect this is a forked process and skip cleanup
    exit(0);
} else {
    // Parent process - wait for child to complete
    echo "Parent: Waiting for child (PID: $pid)\n";
    
    $status = 0;
    pcntl_waitpid($pid, $status);
    
    if (pcntl_wifexited($status)) {
        $exit_code = pcntl_wexitstatus($status);
        echo "Parent: Child exited with code: $exit_code\n";
    } else {
        echo "Parent: Child terminated abnormally\n";
    }
    
    // Now verify parent's resources are still valid
    // This is the critical test - if child's cleanup corrupted parent,
    // this will crash or fail
    echo "\nParent: Verifying connection still works...\n";
    
    $result = fbird_query($conn, "SELECT 1 FROM RDB\$DATABASE");
    if ($result) {
        $row = fbird_fetch_row($result);
        echo "Parent: Query result: " . $row[0] . "\n";
        fbird_free_result($result);
    } else {
        echo "Parent: ERROR - Query failed: " . fbird_errmsg() . "\n";
    }
    
    // Verify transaction is still usable
    echo "\nParent: Verifying transaction still works...\n";
    $result2 = fbird_query($trans, "SELECT 2 FROM RDB\$DATABASE");
    if ($result2) {
        $row = fbird_fetch_row($result2);
        echo "Parent: Transaction query result: " . $row[0] . "\n";
        fbird_free_result($result2);
    } else {
        echo "Parent: ERROR - Transaction query failed: " . fbird_errmsg() . "\n";
    }
    
    // Clean up
    echo "\nParent: Cleaning up...\n";
    fbird_rollback($trans);
    fbird_close($conn);
    
    echo "\n=== Fork safety test completed successfully ===\n";
}
?>
--EXPECTF--
=== UAF: Fork Safety Test ===

Parent: Created connection (PID: %d)
Parent: Created transaction
%AChild: Started (PID: %d)
Child: Exiting normally
Parent: Waiting for child (PID: %d)
Parent: Child exited with code: 0

Parent: Verifying connection still works...
Parent: Query result: 1

Parent: Verifying transaction still works...
Parent: Transaction query result: 2

Parent: Cleaning up...

=== Fork safety test completed successfully ===

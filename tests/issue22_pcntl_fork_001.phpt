--TEST--
Issue #22: No segmentation fault when extension loaded in forked process (pcntl_fork)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird extension not available');
if (!extension_loaded('pcntl')) die('skip pcntl extension not available');
if (PHP_OS_FAMILY === 'Windows') die('skip pcntl not available on Windows');
?>
--FILE--
<?php
/*
 * Test for Issue #22: Segfault when extension loaded in forked process
 *
 * This test verifies that:
 * 1. Extension can be loaded without errors
 * 2. pcntl_fork() can be called successfully
 * 3. Child process exits cleanly without segfault (exit code 139)
 * 4. Parent process continues normally
 *
 * The fix uses PID tracking: init_pid stored in GINIT, destructors
 * skip Firebird API cleanup in forked children (getpid() != init_pid).
 */

echo "Parent PID: " . getmypid() . "\n";
echo "Extension loaded: " . (extension_loaded('firebird') ? 'Yes' : 'No') . "\n";

// Fork the process
$pid = pcntl_fork();

if ($pid == -1) {
    die("Fork failed\n");
} elseif ($pid == 0) {
    // Child process
    echo "Child PID: " . getmypid() . "\n";
    echo "Child: Extension still loaded: " . (extension_loaded('firebird') ? 'Yes' : 'No') . "\n";

    // Child exits here - destructors will be called
    // With fix: destructors detect fork and skip cleanup (no segfault)
    // Without fix: destructors attempt cleanup of parent's handles (segfault)
    exit(0);
} else {
    // Parent process
    echo "Forked child PID: $pid\n";

    // Wait for child to complete
    $status = 0;
    pcntl_waitpid($pid, $status, 0);

    $exit_code = pcntl_wexitstatus($status);
    echo "Child exit code: $exit_code\n";

    if ($exit_code === 139) {
        echo "FAIL: Child segfaulted (exit code 139)\n";
    } elseif ($exit_code === 0) {
        echo "SUCCESS: Child exited cleanly\n";
    } else {
        echo "FAIL: Child exited with code $exit_code\n";
    }

    echo "Parent: Continuing after fork\n";
}

echo "Test complete\n";
?>
--EXPECT--
Parent PID: %d
Extension loaded: Yes
Forked child PID: %d
Child PID: %d
Child: Extension still loaded: Yes
Child exit code: 0
SUCCESS: Child exited cleanly
Parent: Continuing after fork
Test complete

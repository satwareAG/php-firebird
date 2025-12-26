<?php
echo "Parent PID: " . getmypid() . "\n";
echo "Extension loaded: " . (extension_loaded('fbird') ? 'Yes' : 'No') . "\n";

$pid = pcntl_fork();

if ($pid == -1) {
    die("Fork failed\n");
} elseif ($pid == 0) {
    echo "Child PID: " . getmypid() . "\n";
    echo "Child: Extension still loaded: " . (extension_loaded('fbird') ? 'Yes' : 'No') . "\n";
    exit(0);
} else {
    echo "Forked child PID: $pid\n";
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

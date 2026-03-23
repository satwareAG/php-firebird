--TEST--
Issue #119: Regression in fbird_trans_start() with cached connections
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Connecting with pconnect (cached)...\n";
$conn1 = fbird_pconnect($test_base, $user, $password);
if (!$conn1) die("Connection 1 failed: " . fbird_errmsg());

echo "Starting transaction on conn1...\n";
$tr1 = fbird_trans_start($conn1, [FBIRD_WRITE]);
if ($tr1) {
    echo "OK: Transaction 1 started\n";
    fbird_commit($tr1);
} else {
    echo "FAIL: Transaction 1 failed: " . fbird_errmsg() . "\n";
}

fbird_close($conn1);

echo "\nConnecting again (should be from cache)...\n";
$conn2 = fbird_pconnect($test_base, $user, $password);
if (!$conn2) die("Connection 2 failed: " . fbird_errmsg());

echo "Starting transaction on conn2...\n";
$tr2 = fbird_trans_start($conn2, [FBIRD_WRITE]);
if ($tr2) {
    echo "OK: Transaction 2 started\n";
    fbird_commit($tr2);
} else {
    echo "FAIL: Transaction 2 failed: " . fbird_errmsg() . "\n";
}

fbird_close($conn2);

?>
--EXPECTF--
Connecting with pconnect (cached)...
Starting transaction on conn1...
OK: Transaction 1 started

Connecting again (should be from cache)...
Starting transaction on conn2...
OK: Transaction 2 started

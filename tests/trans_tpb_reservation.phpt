--TEST--
fbird_trans_start() with Table Reservation (Locking)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$db = fbird_connect($test_base);

// Setup table
$t = fbird_trans($db);
fbird_query($t, "RECREATE TABLE reservation_test (id INT)");
fbird_commit($t);

// Test 1: Exclusive Write Lock
// This should succeed if no one else is using it (which is true here)
echo "Test 1: Exclusive Write Lock\n";
$options = [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_CONCURRENCY,
    'tables' => [
        'RESERVATION_TEST' => FBIRD_LOCK_WRITE | FBIRD_LOCK_EXCLUSIVE
    ]
];

$trans = fbird_trans_start($db, $options);
if ($trans) {
    echo "Transaction started successfully\n";

    // Verify we can write
    fbird_query($trans, "INSERT INTO reservation_test VALUES (1)");
    echo "Insert successful\n";

    fbird_commit($trans);
    echo "Commit successful\n";
} else {
    echo "Transaction failed\n";
}

// Test 2: Protected Read Lock
echo "Test 2: Protected Read Lock\n";
$options2 = [
    'tables' => [
        'RESERVATION_TEST' => FBIRD_LOCK_READ | FBIRD_LOCK_PROTECTED
    ]
];
$trans2 = fbird_trans_start($db, $options2);
if ($trans2) {
    echo "Transaction started successfully\n";
    fbird_commit($trans2);
} else {
    echo "Transaction failed\n";
}

fbird_close($db);

?>
--EXPECT--
Test 1: Exclusive Write Lock
Transaction started successfully
Insert successful
Commit successful
Test 2: Protected Read Lock
Transaction started successfully

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE reservation_test");
    @fbird_close($db);
}
?>

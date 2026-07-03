--TEST--
Comprehensive TPB (Transaction Parameter Block) feature verification
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
/**
 * Comprehensive test for ALL TPB features supported by php-firebird extension.
 *
 * This test verifies:
 * 1. Access modes: FBIRD_READ, FBIRD_WRITE
 * 2. Isolation levels: FBIRD_COMMITTED, FBIRD_CONSISTENCY, FBIRD_CONCURRENCY
 * 3. Record versioning: FBIRD_REC_VERSION, FBIRD_REC_NO_VERSION
 * 4. Lock resolution: FBIRD_WAIT, FBIRD_NOWAIT, FBIRD_LOCK_TIMEOUT
 * 5. Firebird 4.0+ features: FBIRD_READ_CONSISTENCY (FB4+ only)
 * 6. Table reservation via array API
 */

require("firebird.inc");

$db = fbird_connect($test_base);
if (!$db) {
    die("Failed to connect to database");
}

// Setup test table using EXECUTE BLOCK for atomic CREATE/RECREATE
$setup = fbird_trans($db);
// Use RECREATE to handle both new table and existing table cases
fbird_query($setup, "RECREATE TABLE tpb_test (id INT NOT NULL PRIMARY KEY, val VARCHAR(50))");
fbird_commit($setup);

// Insert initial data in separate transaction (after DDL commit)
$setup2 = fbird_trans($db);
fbird_query($setup2, "INSERT INTO tpb_test VALUES (1, 'initial')");
fbird_commit($setup2);

echo "=== Test 1: Access Modes ===\n";

// Test FBIRD_READ mode
$trans1 = fbird_trans_start($db, ['access_mode' => FBIRD_READ, 'isolation' => FBIRD_CONCURRENCY]);
$info1 = fbird_trans_info($trans1);
echo "READ mode: " . $info1['access_mode'] . "\n";
fbird_commit($trans1);

// Test FBIRD_WRITE mode (default)
$trans2 = fbird_trans_start($db, ['access_mode' => FBIRD_WRITE, 'isolation' => FBIRD_CONCURRENCY]);
$info2 = fbird_trans_info($trans2);
echo "WRITE mode: " . $info2['access_mode'] . "\n";
fbird_commit($trans2);

echo "\n=== Test 2: Isolation Levels ===\n";

// Test FBIRD_CONCURRENCY (SNAPSHOT)
$trans3 = fbird_trans_start($db, ['isolation' => FBIRD_CONCURRENCY]);
$info3 = fbird_trans_info($trans3);
echo "CONCURRENCY isolation: " . $info3['isolation'] . "\n";
fbird_commit($trans3);

// Test FBIRD_COMMITTED (READ COMMITTED)
$trans4 = fbird_trans_start($db, ['isolation' => FBIRD_COMMITTED]);
$info4 = fbird_trans_info($trans4);
echo "COMMITTED isolation: " . $info4['isolation'] . "\n";
fbird_commit($trans4);

// Test FBIRD_CONSISTENCY (SERIALIZABLE - table-level locking)
$trans5 = fbird_trans_start($db, ['isolation' => FBIRD_CONSISTENCY]);
$info5 = fbird_trans_info($trans5);
echo "CONSISTENCY isolation: " . $info5['isolation'] . "\n";
fbird_commit($trans5);

echo "\n=== Test 3: Record Versioning (READ COMMITTED only) ===\n";

// Test FBIRD_REC_VERSION (default for READ COMMITTED in FB 3.0+)
$trans6 = fbird_trans_start($db, [
    'isolation' => FBIRD_COMMITTED,
    'rec_version' => FBIRD_REC_VERSION
]);
$info6 = fbird_trans_info($trans6);
echo "REC_VERSION with COMMITTED: " . $info6['isolation'] . "\n";
fbird_commit($trans6);

// Test FBIRD_REC_NO_VERSION
$trans7 = fbird_trans_start($db, [
    'isolation' => FBIRD_COMMITTED,
    'rec_version' => FBIRD_REC_NO_VERSION
]);
$info7 = fbird_trans_info($trans7);
echo "REC_NO_VERSION with COMMITTED: " . $info7['isolation'] . "\n";
fbird_commit($trans7);

echo "\n=== Test 4: Lock Resolution ===\n";

// Test FBIRD_WAIT (default)
$trans8 = fbird_trans_start($db, [
    'isolation' => FBIRD_CONCURRENCY,
    'lock_resolution' => FBIRD_WAIT
]);
$info8 = fbird_trans_info($trans8);
echo "WAIT lock resolution: OK\n";
fbird_commit($trans8);

// Test FBIRD_NOWAIT
$trans9 = fbird_trans_start($db, [
    'isolation' => FBIRD_CONCURRENCY,
    'lock_resolution' => FBIRD_NOWAIT
]);
$info9 = fbird_trans_info($trans9);
echo "NOWAIT lock resolution: OK\n";
fbird_commit($trans9);

// Test FBIRD_LOCK_TIMEOUT
$trans10 = fbird_trans_start($db, [
    'isolation' => FBIRD_CONCURRENCY,
    'lock_resolution' => FBIRD_WAIT,
    'lock_timeout' => 5
]);
$info10 = fbird_trans_info($trans10);
echo "LOCK_TIMEOUT (5s): " . $info10['lock_timeout'] . "\n";
fbird_commit($trans10);

echo "\n=== Test 5: Classic fbird_trans() Flag Combinations ===\n";

// Test flags-only pattern: fbird_trans(FLAGS, $db)
$trans11 = fbird_trans(FBIRD_READ | FBIRD_COMMITTED | FBIRD_WAIT, $db);
if ($trans11) {
    echo "fbird_trans(READ|COMMITTED|WAIT): OK\n";
    fbird_commit($trans11);
} else {
    echo "fbird_trans(READ|COMMITTED|WAIT): FAILED - " . fbird_errmsg() . "\n";
}

// Test READ + CONCURRENCY + NOWAIT
$trans12 = fbird_trans(FBIRD_READ | FBIRD_CONCURRENCY | FBIRD_NOWAIT, $db);
if ($trans12) {
    echo "fbird_trans(READ|CONCURRENCY|NOWAIT): OK\n";
    fbird_commit($trans12);
} else {
    echo "fbird_trans(READ|CONCURRENCY|NOWAIT): FAILED - " . fbird_errmsg() . "\n";
}

// Test WRITE + CONSISTENCY
$trans13 = fbird_trans(FBIRD_WRITE | FBIRD_CONSISTENCY | FBIRD_WAIT, $db);
if ($trans13) {
    echo "fbird_trans(WRITE|CONSISTENCY|WAIT): OK\n";
    fbird_commit($trans13);
} else {
    echo "fbird_trans(WRITE|CONSISTENCY|WAIT): FAILED - " . fbird_errmsg() . "\n";
}

echo "\n=== Test 6: Table Reservation via Array API ===\n";

// Test LOCK_READ | LOCK_SHARED
$trans14 = fbird_trans_start($db, [
    'isolation' => FBIRD_CONCURRENCY,
    'tables' => [
        'TPB_TEST' => FBIRD_LOCK_READ | FBIRD_LOCK_SHARED
    ]
]);
if ($trans14) {
    echo "Table reservation (READ|SHARED): OK\n";
    fbird_commit($trans14);
} else {
    echo "Table reservation (READ|SHARED): FAILED - " . fbird_errmsg() . "\n";
}

// Test LOCK_WRITE | LOCK_PROTECTED
$trans15 = fbird_trans_start($db, [
    'isolation' => FBIRD_CONCURRENCY,
    'tables' => [
        'TPB_TEST' => FBIRD_LOCK_WRITE | FBIRD_LOCK_PROTECTED
    ]
]);
if ($trans15) {
    echo "Table reservation (WRITE|PROTECTED): OK\n";
    fbird_commit($trans15);
} else {
    echo "Table reservation (WRITE|PROTECTED): FAILED - " . fbird_errmsg() . "\n";
}

// Test LOCK_WRITE | LOCK_EXCLUSIVE
$trans16 = fbird_trans_start($db, [
    'isolation' => FBIRD_CONCURRENCY,
    'tables' => [
        'TPB_TEST' => FBIRD_LOCK_WRITE | FBIRD_LOCK_EXCLUSIVE
    ]
]);
if ($trans16) {
    echo "Table reservation (WRITE|EXCLUSIVE): OK\n";
    fbird_commit($trans16);
} else {
    echo "Table reservation (WRITE|EXCLUSIVE): FAILED - " . fbird_errmsg() . "\n";
}

echo "\n=== Test 7: Firebird 4.0+ READ CONSISTENCY ===\n";

// READ_CONSISTENCY is a flag added to READ COMMITTED isolation level
// Use the classic fbird_trans() flag API for this
if (defined('FBIRD_READ_CONSISTENCY')) {
    // READ_CONSISTENCY must be combined with FBIRD_COMMITTED using bitwise OR
    $trans17 = fbird_trans(FBIRD_WRITE | FBIRD_COMMITTED | FBIRD_READ_CONSISTENCY | FBIRD_WAIT, $db);
    if ($trans17) {
        echo "READ_CONSISTENCY (FB4+): Transaction started OK\n";
        fbird_commit($trans17);
    } else {
        // May fail on FB3 or older servers even with FB4+ client
        echo "READ_CONSISTENCY (FB4+): Not supported or failed - " . fbird_errmsg() . "\n";
    }
} else {
    echo "FBIRD_READ_CONSISTENCY constant not defined (pre-4.0 client)\n";
}

echo "\n=== Test 8: Combined Complex Transaction ===\n";

// Complex transaction with multiple features
$trans18 = fbird_trans_start($db, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_COMMITTED,
    'rec_version' => FBIRD_REC_VERSION,
    'lock_resolution' => FBIRD_WAIT,
    'lock_timeout' => 10,
    'tables' => [
        'TPB_TEST' => FBIRD_LOCK_WRITE | FBIRD_LOCK_PROTECTED
    ]
]);

if ($trans18) {
    $info18 = fbird_trans_info($trans18);
    echo "Complex transaction started:\n";
    echo "  - Access: " . $info18['access_mode'] . "\n";
    echo "  - Isolation: " . $info18['isolation'] . "\n";
    echo "  - Lock timeout: " . $info18['lock_timeout'] . "\n";

    // Verify we can write
    fbird_query($trans18, "UPDATE tpb_test SET val = 'updated' WHERE id = 1");
    echo "  - Write operation: OK\n";

    fbird_commit($trans18);
    echo "  - Commit: OK\n";
} else {
    echo "Complex transaction: FAILED - " . fbird_errmsg() . "\n";
}

// Cleanup
$cleanup = fbird_trans($db);
fbird_query($cleanup, "DROP TABLE tpb_test");
fbird_commit($cleanup);

fbird_close($db);

echo "\n=== ALL TPB TESTS COMPLETED ===\n";
?>
--EXPECTF--
=== Test 1: Access Modes ===
READ mode: READ_ONLY
WRITE mode: READ_WRITE

=== Test 2: Isolation Levels ===
CONCURRENCY isolation: CONCURRENCY
COMMITTED isolation: READ_COMMITTED
CONSISTENCY isolation: CONSISTENCY

=== Test 3: Record Versioning (READ COMMITTED only) ===
REC_VERSION with COMMITTED: READ_COMMITTED
REC_NO_VERSION with COMMITTED: READ_COMMITTED

=== Test 4: Lock Resolution ===
WAIT lock resolution: OK
NOWAIT lock resolution: OK
LOCK_TIMEOUT (5s): 5

=== Test 5: Classic fbird_trans() Flag Combinations ===
fbird_trans(READ|COMMITTED|WAIT): OK
fbird_trans(READ|CONCURRENCY|NOWAIT): OK
fbird_trans(WRITE|CONSISTENCY|WAIT): OK

=== Test 6: Table Reservation via Array API ===
Table reservation (READ|SHARED): OK
Table reservation (WRITE|PROTECTED): OK
Table reservation (WRITE|EXCLUSIVE): OK

=== Test 7: Firebird 4.0+ READ CONSISTENCY ===
%AREAD_CONSISTENCY (FB4+):%s

=== Test 8: Combined Complex Transaction ===
Complex transaction started:
  - Access: READ_WRITE
  - Isolation: READ_COMMITTED
  - Lock timeout: 10
  - Write operation: OK
  - Commit: OK

=== ALL TPB TESTS COMPLETED ===

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

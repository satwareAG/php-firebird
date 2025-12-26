<?php
/**
 * Transaction Stress Sanitizer Test
 * 
 * Tests multiple transactions, commits, and rollbacks to ensure
 * transaction handles are properly managed and freed.
 */

require_once __DIR__ . '/../config.inc';

echo "ASan Transaction Test: Starting...\n";

$dbh = fbird_connect($test_base, $user, $password);
if (!$dbh) die("Connect failed");

// Loop through transactions
for ($i = 0; $i < 10; $i++) {
    $trans = fbird_trans($dbh);
    fbird_query($trans, "SELECT 1 FROM RDB\$DATABASE");
    
    if ($i % 2 == 0) {
        fbird_commit($trans);
    } else {
        fbird_rollback($trans);
    }
}
echo "Transactions cycled.\n";

fbird_close($dbh);
echo "ASan Transaction Test: Completed.\n";

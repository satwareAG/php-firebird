<?php
// Seed: Transaction patterns
// Exercises transaction isolation levels and commit/rollback

$harness->registerOperation('seed_transaction', function() use ($harness) {
    $conn = $harness->getRandomConnection();
    if ($conn) {
        // Default transaction
        $t1 = fbird_trans($conn);
        if ($t1) {
            fbird_commit($t1);
        }
        
        // Explicit parameters
        $t2 = fbird_trans($conn, IBASE_WRITE, IBASE_COMMITTED, IBASE_REC_VERSION);
        if ($t2) {
            $harness->state['transactions'][] = $t2;
        }
        
        // Read-only transaction
        $t3 = fbird_trans($conn, IBASE_READ, IBASE_COMMITTED);
        if ($t3) {
            fbird_rollback($t3);
        }
    }
}, 10.0);

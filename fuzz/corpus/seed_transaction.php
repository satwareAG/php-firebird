<?php
// Seed: Transaction patterns
// Exercises transaction isolation levels and commit/rollback
// NOTE: Uses FBIRD_* constants

$harness->registerOperation('seed_transaction', function() use ($harness) {
    $conn = $harness->getRandomConnection();
    if ($conn && is_resource($conn)) {
        // Default transaction
        $t1 = @fbird_trans($conn);
        if ($t1 && is_resource($t1)) {
            @fbird_commit($t1);
        }
        
        // Explicit parameters - use FBIRD_* constants
        $t2 = @fbird_trans($conn, FBIRD_WRITE | FBIRD_COMMITTED | FBIRD_REC_VERSION);
        if ($t2 && is_resource($t2)) {
            $harness->state['transactions'][] = $t2;
        }
        
        // Read-only transaction
        $t3 = @fbird_trans($conn, FBIRD_READ | FBIRD_COMMITTED);
        if ($t3 && is_resource($t3)) {
            @fbird_rollback($t3);
        }
    }
}, 10.0);

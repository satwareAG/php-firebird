<?php
// Seed: Boundary values
// Exercises INT128, NUMERIC precision, and timestamp extremes
// NOTE: Fixed to use connection properly, not just transaction

$harness->registerOperation('seed_boundary', function() use ($harness) {
    $conn = $harness->getRandomConnection();
    if (!$conn || !is_resource($conn)) {
        return;
    }
    
    $trans = @fbird_trans($conn);
    if (!$trans || !is_resource($trans)) {
        return;
    }
    
    // Create temp table for boundary tests (ignore errors if exists)
    @fbird_query($trans, "CREATE GLOBAL TEMPORARY TABLE FUZZ_BOUNDARY (
        ID INT, 
        VAL_INT128 INT128, 
        VAL_NUM NUMERIC(38, 10),
        VAL_TS TIMESTAMP
    ) ON COMMIT DELETE ROWS");
    
    # fbird_prepare takes (connection, query) - use fbird_trans to set default transaction
    $stmt = @fbird_prepare($conn, "INSERT INTO FUZZ_BOUNDARY (ID, VAL_INT128, VAL_NUM, VAL_TS) VALUES (?, ?, ?, ?)");
    if ($stmt && is_resource($stmt)) {
        // Max INT128 (approx)
        @fbird_execute($stmt, 1, "170141183460469231731687303715884105727", 0, "2025-01-01");
        
        // Min INT128
        @fbird_execute($stmt, 2, "-170141183460469231731687303715884105728", 0, "2025-01-01");
        
        // High precision NUMERIC
        @fbird_execute($stmt, 3, 0, "12345678901234567890.1234567890", "2025-01-01");
        
        // Timestamp extremes
        @fbird_execute($stmt, 4, 0, 0, "0001-01-01 00:00:00");
        @fbird_execute($stmt, 5, 0, 0, "9999-12-31 23:59:59");
        
        @fbird_free_query($stmt);
    }
    
    // Commit or rollback
    @fbird_commit($trans);
}, 5.0);

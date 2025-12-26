<?php
// Seed: Boundary values
// Exercises INT128, NUMERIC precision, and timestamp extremes

$harness->registerOperation('seed_boundary', function() use ($harness) {
    $trans = $harness->getRandomTransaction();
    if ($trans) {
        // Create temp table for boundary tests
        @fbird_query($trans, "CREATE GLOBAL TEMPORARY TABLE FUZZ_BOUNDARY (
            ID INT, 
            VAL_INT128 INT128, 
            VAL_NUM NUMERIC(38, 10),
            VAL_TS TIMESTAMP
        ) ON COMMIT DELETE ROWS");
        
        $stmt = fbird_prepare($trans, "INSERT INTO FUZZ_BOUNDARY (ID, VAL_INT128, VAL_NUM, VAL_TS) VALUES (?, ?, ?, ?)");
        if ($stmt) {
            // Max INT128 (approx)
            fbird_execute($stmt, 1, "170141183460469231731687303715884105727", 0, "2025-01-01");
            
            // Min INT128
            fbird_execute($stmt, 2, "-170141183460469231731687303715884105728", 0, "2025-01-01");
            
            // High precision NUMERIC
            fbird_execute($stmt, 3, 0, "12345678901234567890.1234567890", "2025-01-01");
            
            // Timestamp extremes
            fbird_execute($stmt, 4, 0, 0, "0001-01-01 00:00:00");
            fbird_execute($stmt, 5, 0, 0, "9999-12-31 23:59:59");
            
            fbird_free_query($stmt);
        }
    }
}, 5.0);

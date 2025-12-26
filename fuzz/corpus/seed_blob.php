<?php
// Seed: BLOB edge cases
// Exercises zero-length segments, large blobs, and segmented writes

$harness->registerOperation('seed_blob', function() use ($harness) {
    $trans = $harness->getRandomTransaction();
    if ($trans) {
        // Zero-length segment (CORE-1063)
        $b1 = fbird_blob_create($trans);
        if ($b1) {
            fbird_blob_add($b1, ""); // Empty string
            fbird_blob_close($b1);
        }
        
        // Large BLOB (1MB)
        $b2 = fbird_blob_create($trans);
        if ($b2) {
            $chunk = str_repeat("A", 8192);
            for ($i = 0; $i < 128; $i++) {
                fbird_blob_add($b2, $chunk);
            }
            fbird_blob_close($b2);
        }
        
        // Many small segments
        $b3 = fbird_blob_create($trans);
        if ($b3) {
            for ($i = 0; $i < 100; $i++) {
                fbird_blob_add($b3, ".");
            }
            fbird_blob_close($b3);
        }
    }
}, 5.0);

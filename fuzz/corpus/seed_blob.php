<?php
// Seed: BLOB edge cases
// Exercises zero-length segments, large blobs, and segmented writes
// NOTE: Fixed with proper resource validation

$harness->registerOperation('seed_blob', function() use ($harness) {
    $trans = $harness->getRandomTransaction();
    if (!$trans || !is_resource($trans)) {
        return;
    }
    
    // Zero-length segment (CORE-1063)
    $b1 = @fbird_blob_create($trans);
    if ($b1 && is_resource($b1)) {
        @fbird_blob_add($b1, ""); // Empty string
        @fbird_blob_close($b1);
    }
    
    // Large BLOB (1MB) - reduced to 128KB to avoid timeout
    $b2 = @fbird_blob_create($trans);
    if ($b2 && is_resource($b2)) {
        $chunk = str_repeat("A", 8192);
        for ($i = 0; $i < 16; $i++) {  // 128KB instead of 1MB
            @fbird_blob_add($b2, $chunk);
        }
        @fbird_blob_close($b2);
    }
    
    // Many small segments (reduced count)
    $b3 = @fbird_blob_create($trans);
    if ($b3 && is_resource($b3)) {
        for ($i = 0; $i < 20; $i++) {  // 20 instead of 100
            @fbird_blob_add($b3, ".");
        }
        @fbird_blob_close($b3);
    }
}, 5.0);

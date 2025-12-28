<?php

class BlobOps {
    public static function createBlob(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans && is_resource($trans)) {
                $blob = fbird_blob_create($trans);
                if ($blob) {
                    fbird_blob_add($blob, "Initial data");
                    $id = fbird_blob_close($blob);
                    // Store ID for later retrieval? 
                    // For now we just exercise creation
                }
            }
        };
    }

    public static function streamBlob(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans && is_resource($trans)) {
                $blob = fbird_blob_create($trans);
                if ($blob) {
                    // Write in small chunks
                    for ($i = 0; $i < 10; $i++) {
                        fbird_blob_add($blob, str_repeat("x", 100));
                    }
                    fbird_blob_close($blob);
                }
            }
        };
    }

    public static function largeBlob(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans && is_resource($trans)) {
                $blob = fbird_blob_create($trans);
                if ($blob) {
                    // Write larger chunk
                    fbird_blob_add($blob, str_repeat("A", 65535));
                    fbird_blob_close($blob);
                }
            }
        };
    }

    public static function postCommitAccess(FuzzHarness $h): Closure {
        return function() use ($h) {
            // This is a known edge case: accessing BLOB after commit
            $trans = $h->getRandomTransaction();
            if ($trans && is_resource($trans)) {
                $blob = fbird_blob_create($trans);
                if ($blob) {
                    fbird_blob_add($blob, "Data");
                    // Commit transaction while blob is open
                    fbird_commit($trans);
                    
                    // Attempt to use blob handle (should fail gracefully, not crash)
                    try {
                        @fbird_blob_add($blob, "More data");
                        @fbird_blob_close($blob);
                    } catch (Throwable $e) {
                        // Expected failure
                    }
                    
                    // Remove transaction from state since we committed it
                    $key = array_search($trans, $h->state['transactions'], true);
                    if ($key !== false) {
                        unset($h->state['transactions'][$key]);
                        $h->state['transactions'] = array_values($h->state['transactions']);
                    }
                }
            }
        };
    }
}

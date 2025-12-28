<?php

class QueryOps {
    public static function simpleQuery(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            // Validate connection is still a valid resource before using
            if ($conn && is_resource($conn)) {
                // Use catalog query to ensure validity
                $res = @fbird_query($conn, 'SELECT * FROM RDB$DATABASE');
                if ($res && is_resource($res)) {
                    while ($row = @fbird_fetch_assoc($res)) {
                        // Consume result
                    }
                    @fbird_free_result($res);
                }
            }
        };
    }

    public static function prepareExecute(FuzzHarness $h): Closure {
        return function() use ($h) {
            // fbird_prepare takes CONNECTION, not transaction
            $conn = $h->getRandomConnection();
            if ($conn && is_resource($conn)) {
                $stmt = @fbird_prepare($conn, 'SELECT * FROM RDB$RELATIONS WHERE RDB$RELATION_NAME = ?');
                if ($stmt && is_resource($stmt)) {
                    $res = @fbird_execute($stmt, 'RDB$DATABASE');
                    if ($res && is_resource($res)) {
                        while ($row = @fbird_fetch_row($res)) {
                            // Consume
                        }
                        // MUST free result BEFORE freeing statement to avoid UAF
                        @fbird_free_result($res);
                    }
                    @fbird_free_query($stmt);
                }
            }
        };
    }

    public static function parameterizedInsert(FuzzHarness $h): Closure {
        return function() use ($h) {
            // fbird_prepare takes CONNECTION, not transaction
            $conn = $h->getRandomConnection();
            if ($conn && is_resource($conn)) {
                // Start explicit transaction
                $trans = @fbird_trans($conn);
                if (!$trans || !is_resource($trans)) {
                    return;
                }
                
                // Create temp table if not exists (this might fail if already exists, which is fine)
                @fbird_query($trans, "CREATE GLOBAL TEMPORARY TABLE FUZZ_TEST (ID INT, DATA VARCHAR(100)) ON COMMIT DELETE ROWS");
                
                $stmt = @fbird_prepare($conn, "INSERT INTO FUZZ_TEST (ID, DATA) VALUES (?, ?)");
                if ($stmt && is_resource($stmt)) {
                    $res = @fbird_execute($stmt, rand(1, 1000), "Fuzz Data " . rand());
                    // Free result if any before freeing statement
                    if ($res && is_resource($res)) {
                        @fbird_free_result($res);
                    }
                    @fbird_free_query($stmt);
                }
                
                // Commit and don't add to state (transaction is complete)
                @fbird_commit($trans);
            }
        };
    }

    public static function fetchAll(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            // Validate connection is still a valid resource before using
            if ($conn && is_resource($conn)) {
                $res = @fbird_query($conn, 'SELECT first 10 * FROM RDB$TYPES');
                if ($res && is_resource($res)) {
                    // Exercise different fetch modes
                    while ($row = @fbird_fetch_object($res)) {
                        // Consume
                    }
                    @fbird_free_result($res);
                }
            }
        };
    }
}

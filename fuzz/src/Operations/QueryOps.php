<?php

class QueryOps {
    public static function simpleQuery(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if ($conn) {
                // Use catalog query to ensure validity
                $res = fbird_query($conn, 'SELECT * FROM RDB$DATABASE');
                if ($res) {
                    while ($row = fbird_fetch_assoc($res)) {
                        // Consume result
                    }
                    fbird_free_result($res);
                }
            }
        };
    }

    public static function prepareExecute(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans) {
                $stmt = fbird_prepare($trans, 'SELECT * FROM RDB$RELATIONS WHERE RDB$RELATION_NAME = ?');
                if ($stmt) {
                    $res = fbird_execute($stmt, 'RDB$DATABASE');
                    if ($res) {
                        while ($row = fbird_fetch_row($res)) {
                            // Consume
                        }
                    }
                    fbird_free_query($stmt);
                }
            }
        };
    }

    public static function parameterizedInsert(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans) {
                // Create temp table if not exists (this might fail if already exists, which is fine)
                @fbird_query($trans, "CREATE GLOBAL TEMPORARY TABLE FUZZ_TEST (ID INT, DATA VARCHAR(100)) ON COMMIT DELETE ROWS");
                
                $stmt = fbird_prepare($trans, "INSERT INTO FUZZ_TEST (ID, DATA) VALUES (?, ?)");
                if ($stmt) {
                    fbird_execute($stmt, rand(1, 1000), "Fuzz Data " . rand());
                    fbird_free_query($stmt);
                }
            }
        };
    }

    public static function fetchAll(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if ($conn) {
                $res = fbird_query($conn, 'SELECT first 10 * FROM RDB$TYPES');
                if ($res) {
                    // Exercise different fetch modes
                    while ($row = fbird_fetch_object($res)) {
                        // Consume
                    }
                    fbird_free_result($res);
                }
            }
        };
    }
}

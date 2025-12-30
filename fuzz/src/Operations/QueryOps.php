<?php

require_once __DIR__ . '/../SqlGenerator.php';

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

    /**
     * Execute a randomly generated SELECT query against system tables
     * Uses SqlGenerator for random query and parameter generation
     */
    public static function randomSelect(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if (!$conn || !is_resource($conn)) {
                return;
            }

            $generator = new SqlGenerator();
            $query = $generator->generateSelect();

            if (empty($query['params'])) {
                // Direct query without parameters
                $res = @fbird_query($conn, $query['sql']);
            } else {
                // Prepared statement with parameters
                $stmt = @fbird_prepare($conn, $query['sql']);
                if (!$stmt || !is_resource($stmt)) {
                    return;
                }
                $res = @fbird_execute($stmt, ...$query['params']);
                @fbird_free_query($stmt);
            }

            if ($res && is_resource($res)) {
                // Consume results using random fetch method
                $fetchMethods = ['fbird_fetch_assoc', 'fbird_fetch_row', 'fbird_fetch_object'];
                $fetchMethod = $fetchMethods[array_rand($fetchMethods)];
                while ($row = @$fetchMethod($res)) {
                    // Consume
                }
                @fbird_free_result($res);
            }
        };
    }

    /**
     * Execute a randomly generated parameterized query with edge case values
     * Tests parameter binding with NULL, integers, floats, strings, dates
     */
    public static function randomParams(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if (!$conn || !is_resource($conn)) {
                return;
            }

            $generator = new SqlGenerator();

            // Generate 1-5 random parameters
            $paramCount = rand(1, 5);
            $params = $generator->generateRandomParams($paramCount);

            // Build a query that uses these parameters
            $placeholders = implode(', ', array_fill(0, $paramCount, '?'));
            $sql = "SELECT {$placeholders} FROM RDB\$DATABASE";

            $stmt = @fbird_prepare($conn, $sql);
            if (!$stmt || !is_resource($stmt)) {
                return;
            }

            $res = @fbird_execute($stmt, ...$params);
            if ($res && is_resource($res)) {
                @fbird_fetch_row($res);
                @fbird_free_result($res);
            }
            @fbird_free_query($stmt);
        };
    }

    /**
     * Execute INSERT with randomly generated data into temp table
     * Tests parameter binding for INSERT operations
     */
    public static function randomInsert(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if (!$conn || !is_resource($conn)) {
                return;
            }

            // Start explicit transaction
            $trans = @fbird_trans($conn);
            if (!$trans || !is_resource($trans)) {
                return;
            }

            // Ensure temp table exists
            @fbird_query($trans, "CREATE GLOBAL TEMPORARY TABLE FUZZ_RANDOM (
                ID INTEGER,
                DATA VARCHAR(255),
                NUM DOUBLE PRECISION,
                DT TIMESTAMP
            ) ON COMMIT DELETE ROWS");

            $generator = new SqlGenerator();

            // Generate random values for each column
            $id = $generator->generateRandomValue('integer');
            $data = $generator->generateRandomValue('string');
            $num = $generator->generateRandomValue('float');
            $dt = $generator->generateRandomValue('timestamp');

            $stmt = @fbird_prepare($conn, "INSERT INTO FUZZ_RANDOM (ID, DATA, NUM, DT) VALUES (?, ?, ?, ?)");
            if ($stmt && is_resource($stmt)) {
                $res = @fbird_execute($stmt, $id, $data, $num, $dt);
                if ($res && is_resource($res)) {
                    @fbird_free_result($res);
                }
                @fbird_free_query($stmt);
            }

            @fbird_commit($trans);
        };
    }

    /**
     * Test edge case string values that may trigger escaping issues
     * Specifically targets firebird_utils.cpp string handling
     */
    public static function edgeCaseStrings(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if (!$conn || !is_resource($conn)) {
                return;
            }

            $generator = new SqlGenerator();

            // Edge case strings that may trigger issues
            $edgeCases = [
                '',                          // Empty string
                "\0",                        // Null byte
                "'\\'\"",                    // Quotes and backslash
                str_repeat('A', 32767),      // Max VARCHAR size
                $generator->generateRandomUnicodeString(100), // Unicode
                $generator->generateRandomBinary(100),        // Binary data
                "SELECT * FROM RDB\$DATABASE", // SQL injection attempt
                "'; DROP TABLE test1; --",   // SQL injection attempt
                "\n\r\t",                    // Whitespace characters
            ];

            $testValue = $edgeCases[array_rand($edgeCases)];

            $stmt = @fbird_prepare($conn, "SELECT ? FROM RDB\$DATABASE");
            if ($stmt && is_resource($stmt)) {
                $res = @fbird_execute($stmt, $testValue);
                if ($res && is_resource($res)) {
                    @fbird_fetch_row($res);
                    @fbird_free_result($res);
                }
                @fbird_free_query($stmt);
            }
        };
    }
}
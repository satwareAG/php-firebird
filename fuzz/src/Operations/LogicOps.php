<?php

class LogicOps {
    private static bool $tableCreated = false;

    public static function tlpCheck(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            // Validate transaction is still a valid resource
            if (!$trans || !is_resource($trans)) return;

            // 1. Setup Table (once per run ideally, but we check flag)
            if (!self::$tableCreated) {
                // Try to drop first to ensure clean state, ignore errors
                @fbird_query($trans, "DROP TABLE FUZZ_TLP");
                @fbird_query($trans, "CREATE TABLE FUZZ_TLP (ID INT, VAL INT, TEXT_VAL VARCHAR(50))");
                @fbird_commit($trans); // Commit DDL
                
                // Remove old transaction from state
                $key = array_search($trans, $h->state['transactions'], true);
                if ($key !== false) {
                    unset($h->state['transactions'][$key]);
                    $h->state['transactions'] = array_values($h->state['transactions']);
                }
                
                // Start new transaction for data
                $conn = $h->getRandomConnection();
                if (!$conn || !is_resource($conn)) return;
                $trans = @fbird_trans($conn);
                if ($trans) {
                    $h->state['transactions'][] = $trans;
                    self::$tableCreated = true;
                } else {
                    return;
                }
            }

            // 2. Insert Random Data
            // Use @ suppressor - transaction may be invalid if connection was closed
            $stmt = @fbird_prepare($trans, "INSERT INTO FUZZ_TLP (ID, VAL, TEXT_VAL) VALUES (?, ?, ?)");
            if ($stmt) {
                for ($i = 0; $i < 10; $i++) {
                    $id = rand(1, 1000);
                    $val = rand(0, 1) ? rand(1, 100) : null;
                    $text = rand(0, 1) ? "Text_" . rand(1, 100) : null;
                    @fbird_execute($stmt, $id, $val, $text);
                }
                @fbird_free_query($stmt);
            }

            // 3. Generate Predicate P
            // Simple predicates for now to avoid syntax errors
            $predicates = [
                "VAL > 50",
                "VAL IS NULL",
                "VAL = 10",
                "TEXT_VAL LIKE 'Text_%'",
                "TEXT_VAL IS NULL",
                "ID < 500"
            ];
            $p = $predicates[array_rand($predicates)];

            // 4. Run TLP Queries
            $q1 = self::getCount($trans, "SELECT COUNT(*) FROM FUZZ_TLP WHERE $p");
            $q2 = self::getCount($trans, "SELECT COUNT(*) FROM FUZZ_TLP WHERE NOT ($p)");
            $q3 = self::getCount($trans, "SELECT COUNT(*) FROM FUZZ_TLP WHERE ($p) IS NULL");
            $total = self::getCount($trans, "SELECT COUNT(*) FROM FUZZ_TLP");

            // 5. Verify Logic
            $sum = $q1 + $q2 + $q3;
            if ($sum !== $total) {
                throw new Exception("Logic Bug Detected (TLP)! Predicate: [$p]. Q1($q1) + Q2($q2) + Q3($q3) = $sum != Total($total)");
            }
        };
    }

    private static function getCount($trans, $sql): int {
        if (!$trans || !is_resource($trans)) return 0;
        $res = @fbird_query($trans, $sql);
        if ($res) {
            $row = @fbird_fetch_row($res);
            @fbird_free_result($res);
            return $row ? (int)$row[0] : 0;
        }
        return 0;
    }

    public static function typeCheck(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            // Validate connection is still a valid resource
            if (!$conn || !is_resource($conn)) return;

            // Query known types from system tables
            // Use @ suppressor - connection may be invalid after close operations
            $res = @fbird_query($conn, 'SELECT count(*) as CNT, cast(1.5 as float) as FLT, \'test\' as STR FROM RDB$DATABASE');
            if ($res) {
                $obj = @fbird_fetch_object($res);
                @fbird_free_result($res);
                
                if (!$obj) return;

                if (!is_int($obj->CNT) && !is_string($obj->CNT)) { // Firebird might return count as int64 (string in PHP on 32bit) or int
                     // Allow string for bigints, but check it's numeric
                     if (is_string($obj->CNT) && !is_numeric($obj->CNT)) {
                         throw new Exception("Type Mismatch: CNT expected numeric, got " . gettype($obj->CNT));
                     }
                }
                
                // Float check
                if (abs($obj->FLT - 1.5) > 0.0001) {
                     throw new Exception("Value Mismatch: FLT expected 1.5, got $obj->FLT");
                }
                
                // String check
                if ($obj->STR !== 'test') {
                    throw new Exception("Value Mismatch: STR expected 'test', got '$obj->STR'");
                }
            }
        };
    }
}

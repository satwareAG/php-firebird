<?php

class TransactionOps {
    public static function begin(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if ($conn) {
                $trans = fbird_trans($conn, IBASE_WRITE, IBASE_COMMITTED, IBASE_REC_VERSION);
                if ($trans) {
                    $h->state['transactions'][] = $trans;
                }
            }
        };
    }

    public static function commit(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans) {
                fbird_commit($trans);
                // Remove from state
                $key = array_search($trans, $h->state['transactions'], true);
                if ($key !== false) {
                    unset($h->state['transactions'][$key]);
                    $h->state['transactions'] = array_values($h->state['transactions']);
                }
            }
        };
    }

    public static function rollback(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans) {
                fbird_rollback($trans);
                // Remove from state
                $key = array_search($trans, $h->state['transactions'], true);
                if ($key !== false) {
                    unset($h->state['transactions'][$key]);
                    $h->state['transactions'] = array_values($h->state['transactions']);
                }
            }
        };
    }

    public static function commitRetaining(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans) {
                // Keeps transaction open
                fbird_commit_ret($trans);
            }
        };
    }
}

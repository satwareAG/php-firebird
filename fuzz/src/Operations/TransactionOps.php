<?php

class TransactionOps {
    public static function begin(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if ($conn && is_resource($conn)) {
                $trans = @fbird_trans($conn, FBIRD_WRITE | FBIRD_COMMITTED | FBIRD_REC_VERSION);
                if ($trans) {
                    $h->state['transactions'][] = $trans;
                }
            }
        };
    }

    public static function commit(FuzzHarness $h): Closure {
        return function() use ($h) {
            $trans = $h->getRandomTransaction();
            if ($trans && is_resource($trans)) {
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
            if ($trans && is_resource($trans)) {
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
            if ($trans && is_resource($trans)) {
                // Keeps transaction open
                fbird_commit_ret($trans);
            }
        };
    }
}

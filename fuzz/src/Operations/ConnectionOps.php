<?php

class ConnectionOps {
    public static function connect(FuzzHarness $h): Closure {
        return function() use ($h) {
            // Use FBIRD_CONNECT_FORCE_NEW to ensure each call creates a unique handle
            // This prevents resource reuse which causes use-after-free in cleanup
            $conn = @fbird_connect($h->getDsn(), $h->getUser(), $h->getPassword(), 'UTF8', 0, 3, '', FBIRD_CONNECT_FORCE_NEW);
            if ($conn && is_resource($conn)) {
                // Check for duplicates before adding (resource comparison)
                $isDuplicate = false;
                foreach ($h->state['connections'] as $existing) {
                    if ($existing === $conn) {
                        $isDuplicate = true;
                        break;
                    }
                }
                if (!$isDuplicate) {
                    $h->state['connections'][] = $conn;
                }
            }
        };
    }

    public static function pconnect(FuzzHarness $h): Closure {
        return function() use ($h) {
            // Persistent connections should NOT be stored or closed by fuzzer
            // They persist across requests and closing them causes issues
            $conn = @fbird_pconnect($h->getDsn(), $h->getUser(), $h->getPassword());
            // Intentionally don't store - persistent connections are managed by PHP
        };
    }

    public static function close(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if ($conn && is_resource($conn)) {
                // First, rollback and remove any transactions started on this connection
                // Transactions become invalid when their connection is closed
                $validTransactions = [];
                foreach ($h->state['transactions'] as $trans) {
                    if (is_resource($trans)) {
                        // Try to rollback - if it fails, the transaction is already invalid
                        // or belongs to a different connection (we can't tell which)
                        @fbird_rollback($trans);
                    }
                }
                // Clear all transactions since we can't reliably determine which belong
                // to which connection. The fuzzer will create new ones.
                $h->state['transactions'] = [];
                
                // Also clear statements since they depend on transactions/connections
                foreach ($h->state['statements'] as $stmt) {
                    if (is_resource($stmt)) {
                        @fbird_free_query($stmt);
                    }
                }
                $h->state['statements'] = [];
                
                // Now close the connection
                @fbird_close($conn);
                
                // Remove from state
                $key = array_search($conn, $h->state['connections'], true);
                if ($key !== false) {
                    unset($h->state['connections'][$key]);
                    $h->state['connections'] = array_values($h->state['connections']);
                }
            }
        };
    }

    public static function forceNew(FuzzHarness $h): Closure {
        return function() use ($h) {
            // Force new connection using FBIRD_CONNECT_FORCE_NEW flag
            $conn = @fbird_connect($h->getDsn(), $h->getUser(), $h->getPassword(), 'UTF8', 0, 3, '', FBIRD_CONNECT_FORCE_NEW);
            if ($conn && is_resource($conn)) {
                $h->state['connections'][] = $conn;
            }
        };
    }
}

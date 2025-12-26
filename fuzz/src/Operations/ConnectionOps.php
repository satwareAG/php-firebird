<?php

class ConnectionOps {
    public static function connect(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = fbird_connect($h->getDsn(), $h->getUser(), $h->getPassword());
            if ($conn) {
                $h->state['connections'][] = $conn;
            }
        };
    }

    public static function pconnect(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = fbird_pconnect($h->getDsn(), $h->getUser(), $h->getPassword());
            if ($conn) {
                // Don't store pconnect in state to avoid closing it, 
                // but we exercise the API
            }
        };
    }

    public static function close(FuzzHarness $h): Closure {
        return function() use ($h) {
            $conn = $h->getRandomConnection();
            if ($conn) {
                fbird_close($conn);
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
            // Force new connection even if parameters match
            // Note: fbird_connect doesn't have a force_new param in standard API,
            // but we can simulate different connection contexts
            $conn = fbird_connect($h->getDsn(), $h->getUser(), $h->getPassword(), 'UTF8');
            if ($conn) {
                $h->state['connections'][] = $conn;
            }
        };
    }
}

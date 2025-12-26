<?php
// Seed: Connection patterns
// Exercises basic connection, pconnect, and force new connection

$harness->registerOperation('seed_connect', function() use ($harness) {
    // Standard connection
    $c1 = fbird_connect($harness->getDsn(), $harness->getUser(), $harness->getPassword());
    if ($c1) {
        $harness->state['connections'][] = $c1;
        
        // Persistent connection
        $c2 = fbird_pconnect($harness->getDsn(), $harness->getUser(), $harness->getPassword());
        
        // Force new connection with charset
        $c3 = fbird_connect($harness->getDsn(), $harness->getUser(), $harness->getPassword(), 'UTF8');
        if ($c3) {
            $harness->state['connections'][] = $c3;
        }
    }
}, 10.0);

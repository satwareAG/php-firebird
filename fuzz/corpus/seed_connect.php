<?php
// Seed: Connection patterns
// Exercises basic connection and force new connection
// NOTE: pconnect is DISABLED - persistent connections share resources causing use-after-free

$harness->registerOperation('seed_connect', function() use ($harness) {
    // CRITICAL: Always use FBIRD_CONNECT_FORCE_NEW to prevent resource sharing
    // Without this, fbird_connect() returns the SAME resource for identical parameters,
    // causing use-after-free when one reference is closed
    
    // Standard connection with FORCE_NEW
    $c1 = @fbird_connect(
        $harness->getDsn(), 
        $harness->getUser(), 
        $harness->getPassword(),
        'UTF8',
        0,
        3,
        '',
        FBIRD_CONNECT_FORCE_NEW
    );
    if ($c1 && is_resource($c1)) {
        $harness->state['connections'][] = $c1;
        
        // DISABLED: pconnect - persistent connections share resources with regular 
        // connections, causing use-after-free when cleanup closes the shared resource.
        // $c2 = fbird_pconnect($harness->getDsn(), $harness->getUser(), $harness->getPassword());
        
        // Force new connection with different charset variation
        $c3 = @fbird_connect(
            $harness->getDsn(), 
            $harness->getUser(), 
            $harness->getPassword(), 
            'NONE',  // Different charset to test variation
            0,
            3,
            '',
            FBIRD_CONNECT_FORCE_NEW
        );
        if ($c3 && is_resource($c3)) {
            $harness->state['connections'][] = $c3;
        }
    }
}, 10.0);

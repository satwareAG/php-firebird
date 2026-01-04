--TEST--
Stress test: All resource types cleaned up at shutdown (Issue #55, #50, #51)
--DESCRIPTION--
Comprehensive stress test targeting shutdown crashes (exit code 139).
Exercises ALL destructor paths:
- Persistent connections (le_plink) via fbird_pconnect()
- Normal connections (le_link) via fbird_connect()
- Explicit transactions (le_trans) via fbird_trans()
- Batches (le_batch) via fbird_batch_create() [FB 4.0+ only]
- ReflectionExtension for metadata loading
- Multiple iterations to stress memory allocator

IMPORTANT: No explicit cleanup - forces destructors at MSHUTDOWN/RSHUTDOWN.
If this test crashes with exit code 139 (SIGSEGV), run under:
  ./scripts/debug_segfault.sh --valgrind tests/stress_shutdown.phpt
  ./scripts/debug_segfault.sh --gdb tests/stress_shutdown.phpt
--EXTENSIONS--
firebird
--SKIPIF--
<?php
if (!extension_loaded("firebird")) die("skip firebird extension not available");
require_once("firebird.inc");
if (!@fbird_connect($test_base)) {
    die("skip: cannot connect to test database");
}
?>
--FILE--
<?php
require_once("firebird.inc");

echo "=== Stress Shutdown Test ===\n";
echo "Testing destructor paths for Issue #55, #50, #51\n\n";

$connections = [];
$pconnections = [];
$transactions = [];
$batches = [];

// Configuration
$NUM_NORMAL_CONNS = 3;
$NUM_PERSISTENT_CONNS = 3;
$NUM_TRANSACTIONS = 5;
$NUM_ITERATIONS = 3;

// Detect Firebird 4.0+ for batch API
$has_batch_api = function_exists('fbird_batch_create');
$fb_version = 0.0;
try {
    $fb_version = get_fb_version();
} catch (Exception $e) {
    // Ignore version detection failures
}

echo "Environment:\n";
echo "  Firebird server version: " . ($fb_version > 0 ? $fb_version : "unknown") . "\n";
echo "  Batch API available: " . ($has_batch_api ? "yes" : "no") . "\n";
echo "  Normal connections: $NUM_NORMAL_CONNS\n";
echo "  Persistent connections: $NUM_PERSISTENT_CONNS\n";
echo "  Transactions per connection: $NUM_TRANSACTIONS\n";
echo "  Stress iterations: $NUM_ITERATIONS\n\n";

// ============================================================================
// Phase 1: ReflectionExtension (loads extension metadata)
// ============================================================================
echo "Phase 1: ReflectionExtension\n";
$ref = new ReflectionExtension('firebird');
$functions = $ref->getFunctions();
echo "  Loaded " . count($functions) . " functions\n";
$classes = $ref->getClassNames();
echo "  Loaded " . count($classes) . " classes\n";
unset($ref, $functions, $classes);
echo "  Done\n\n";

// ============================================================================
// Phase 2: Normal connections
// ============================================================================
echo "Phase 2: Normal connections\n";
for ($i = 0; $i < $NUM_NORMAL_CONNS; $i++) {
    $conn = fbird_connect($test_base);
    if ($conn) {
        $connections[] = $conn;
        echo "  Connection $i: OK\n";
        
        // Simple query to ensure connection is active
        $result = @fbird_query($conn, 'SELECT 1 FROM RDB$DATABASE');
        if ($result) {
            $row = fbird_fetch_row($result);
            fbird_free_result($result);
        }
    } else {
        echo "  Connection $i: FAILED - " . fbird_errmsg() . "\n";
    }
}
echo "  Total: " . count($connections) . " connections\n\n";

// ============================================================================
// Phase 3: Persistent connections
// ============================================================================
echo "Phase 3: Persistent connections\n";
for ($i = 0; $i < $NUM_PERSISTENT_CONNS; $i++) {
    $pconn = fbird_pconnect($test_base);
    if ($pconn) {
        $pconnections[] = $pconn;
        echo "  Persistent connection $i: OK\n";
        
        // Simple query to ensure connection is active
        $result = @fbird_query($pconn, 'SELECT 1 FROM RDB$DATABASE');
        if ($result) {
            $row = fbird_fetch_row($result);
            fbird_free_result($result);
        }
    } else {
        echo "  Persistent connection $i: FAILED - " . fbird_errmsg() . "\n";
    }
}
echo "  Total: " . count($pconnections) . " persistent connections\n\n";

// ============================================================================
// Phase 4: Explicit transactions
// ============================================================================
echo "Phase 4: Explicit transactions\n";
$trans_count = 0;
foreach (array_merge($connections, $pconnections) as $idx => $conn) {
    for ($t = 0; $t < $NUM_TRANSACTIONS; $t++) {
        $tr = @fbird_trans($conn);
        if ($tr) {
            $transactions[] = $tr;
            $trans_count++;
            
            // Execute a simple query in the transaction
            @fbird_query($tr, 'SELECT COUNT(*) FROM test1');
        }
    }
}
echo "  Created " . count($transactions) . " transactions\n\n";

// ============================================================================
// Phase 5: Batch API (Firebird 4.0+ only)
// ============================================================================
echo "Phase 5: Batch API (FB 4.0+)\n";
if ($has_batch_api && $fb_version >= 4.0 && count($connections) > 0) {
    $conn = $connections[0];
    $tr = @fbird_trans($conn);
    if ($tr) {
        // Clear test data
        @fbird_query($tr, 'DELETE FROM test1 WHERE i > 1000');
        
        $query = @fbird_prepare($tr, 'INSERT INTO test1 (i, c) VALUES (?, ?)');
        if ($query) {
            $batch = @fbird_batch_create($query, $tr);
            if ($batch) {
                $batches[] = $batch;
                echo "  Created batch object\n";
                
                // Add some rows
                for ($i = 1001; $i <= 1010; $i++) {
                    @fbird_batch_add($batch, $i, "Batch row $i");
                }
                echo "  Added 10 rows to batch\n";
                
                // Execute batch
                $result = @fbird_batch_execute($batch);
                if ($result) {
                    echo "  Batch executed: " . $result['success_count'] . " rows\n";
                } else {
                    echo "  Batch execute failed (non-critical)\n";
                }
            } else {
                echo "  Batch creation not successful (non-critical)\n";
            }
        } else {
            echo "  Prepare failed (non-critical)\n";
        }
        // Note: Transaction NOT committed intentionally
    }
} else {
    echo "  Skipped (requires Firebird 4.0+ and connections)\n";
}
echo "\n";

// ============================================================================
// Phase 6: Stress iterations (allocator stress)
// ============================================================================
echo "Phase 6: Stress iterations\n";
for ($iter = 0; $iter < $NUM_ITERATIONS; $iter++) {
    echo "  Iteration $iter: ";
    
    // Create temporary connection, do work, don't close
    $tmp_conn = @fbird_connect($test_base);
    if ($tmp_conn) {
        // Create transaction
        $tmp_tr = @fbird_trans($tmp_conn);
        if ($tmp_tr) {
            // Execute queries
            for ($q = 0; $q < 10; $q++) {
                @fbird_query($tmp_tr, 'SELECT * FROM test1 WHERE i = ' . $q);
            }
            // Don't commit, don't close - let destructor handle it
        }
        echo "OK\n";
    } else {
        echo "SKIP (connection limit?)\n";
    }
}
echo "\n";

// ============================================================================
// Phase 7: Service attachment (if available)
// ============================================================================
echo "Phase 7: Service attachment\n";
if (function_exists('fbird_service_attach')) {
    $host = getenv('FIREBIRD_HOST') ?: 'localhost';
    $svc = @fbird_service_attach($host, 'SYSDBA', 'masterkey');
    if ($svc) {
        echo "  Service attached\n";
        $version = @fbird_server_info($svc, FBIRD_SVC_SERVER_VERSION);
        if ($version) {
            echo "  Server version retrieved\n";
        }
        // Don't detach - let destructor handle it
    } else {
        echo "  Service not available (non-critical)\n";
    }
} else {
    echo "  Service API not available\n";
}
echo "\n";

// ============================================================================
// Summary
// ============================================================================
echo "=== Summary ===\n";
echo "Resources created (NOT explicitly cleaned up):\n";
echo "  Normal connections: " . count($connections) . "\n";
echo "  Persistent connections: " . count($pconnections) . "\n";
echo "  Transactions: " . count($transactions) . "\n";
echo "  Batches: " . count($batches) . "\n\n";

echo "Now exiting without fbird_close()/fbird_commit()/fbird_rollback()...\n";
echo "If PHP crashes here (exit 139), the bug is in destructor code.\n\n";

echo "Test completed\n";

// CRITICAL: No cleanup calls here!
// This forces:
// - _php_fbird_free_batch() during RSHUTDOWN
// - _php_fbird_free_trans() during RSHUTDOWN  
// - _php_fbird_close_link() during RSHUTDOWN
// - _php_fbird_close_plink() during MSHUTDOWN
//
// The crash (Issue #55) occurs when these destructors run in forked
// worker processes (PHPStan/PHPUnit parallel mode) where the parent
// process's Firebird handles are invalid.
?>
--EXPECTF--
=== Stress Shutdown Test ===
Testing destructor paths for Issue #55, #50, #51

Environment:
  Firebird server version: %s
  Batch API available: %s
  Normal connections: 3
  Persistent connections: 3
  Transactions per connection: 5
  Stress iterations: 3

Phase 1: ReflectionExtension
  Loaded %d functions
  Loaded %d classes
  Done

Phase 2: Normal connections
  Connection 0: OK
  Connection 1: OK
  Connection 2: OK
  Total: 3 connections

Phase 3: Persistent connections
  Persistent connection 0: OK
  Persistent connection 1: OK
  Persistent connection 2: OK
  Total: 3 persistent connections

Phase 4: Explicit transactions
  Created %d transactions

Phase 5: Batch API (FB 4.0+)
%s

Phase 6: Stress iterations
  Iteration 0: OK
  Iteration 1: OK
  Iteration 2: OK

Phase 7: Service attachment
  Service %s
%A
=== Summary ===
Resources created (NOT explicitly cleaned up):
  Normal connections: 3
  Persistent connections: 3
  Transactions: %d
  Batches: %d

Now exiting without fbird_close()/fbird_commit()/fbird_rollback()...
If PHP crashes here (exit 139), the bug is in destructor code.

Test completed
%A

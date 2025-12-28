<?php
// Main entry point for PHP Firebird Fuzzer

require_once __DIR__ . '/src/SarifReport.php';
require_once __DIR__ . '/src/FuzzResult.php';
require_once __DIR__ . '/src/FuzzHarness.php';
require_once __DIR__ . '/src/Operations/ConnectionOps.php';
require_once __DIR__ . '/src/Operations/TransactionOps.php';
require_once __DIR__ . '/src/Operations/QueryOps.php';
require_once __DIR__ . '/src/Operations/BlobOps.php';
require_once __DIR__ . '/src/Operations/LogicOps.php';

// Load test helpers for random data generation
require_once __DIR__ . '/../tests/functions.inc';

// Default configuration
$config = [
    'iterations' => 1000,
    'output' => 'fuzz_report.sarif',
    'dsn' => 'localhost:/var/lib/firebird/data/test.fdb',
    'user' => 'SYSDBA',
    'password' => 'masterkey'
];

// Parse arguments
foreach ($argv as $arg) {
    if (strpos($arg, '--iterations=') === 0) {
        $config['iterations'] = (int)substr($arg, 13);
    } elseif (strpos($arg, '--output=') === 0) {
        $config['output'] = substr($arg, 9);
    } elseif (strpos($arg, '--dsn=') === 0) {
        $config['dsn'] = substr($arg, 6);
    }
}

// Initialize report
$report = new SarifReport();
$runIndex = 0; // Use the default run created by constructor

echo "Starting fuzzer with {$config['iterations']} iterations...\n";

try {
    // Initialize harness
    $harness = new FuzzHarness($config['dsn'], $config['user'], $config['password']);
    
    // Load default operations
    $harness->loadDefaultOperations();
    
    // Load corpus seeds
    foreach (glob(__DIR__ . '/corpus/*.php') as $seedFile) {
        require $seedFile;
    }
    
    // Execute fuzzing
    $result = $harness->execute($config['iterations'], function($current, $total) {
        if ($current % 1000 === 0) {
            echo "Progress: $current / $total\r";
        }
    });
    
    echo "\nFuzzing complete in " . number_format($result->duration, 2) . "s\n";
    echo "Passed: {$result->passed}, Failed: {$result->failed}\n";
    
    // Convert errors to SARIF results
    foreach ($result->errors as $error) {
        $report->addResult(
            $runIndex,
            'FUZZ001',
            'error',
            $error['error']['message'],
            [
                'file' => $error['error']['file'],
                'line' => $error['error']['line']
            ]
        );
    }
    
    $report->setInvocation($runIndex, $result->failed === 0);
    
    // Save report
    file_put_contents($config['output'], $report->toJson());
    echo "Report saved to {$config['output']}\n";
    
    exit($result->failed > 0 ? 1 : 0);

} catch (Throwable $e) {
    echo "Fatal error: " . $e->getMessage() . "\n";
    $report->setInvocation($runIndex, false, 1);
    file_put_contents($config['output'], $report->toJson());
    exit(1);
}

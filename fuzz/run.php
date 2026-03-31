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
    'password' => 'masterkey',
    'dictionary' => null,
];

// Parse arguments
foreach ($argv as $arg) {
    if (strpos($arg, '--iterations=') === 0) {
        $config['iterations'] = (int)substr($arg, 13);
    } elseif (strpos($arg, '--output=') === 0) {
        $config['output'] = substr($arg, 9);
    } elseif (strpos($arg, '--dsn=') === 0) {
        $config['dsn'] = substr($arg, 6);
    } elseif (strpos($arg, '--dictionary=') === 0) {
        $config['dictionary'] = substr($arg, 13);
    }
}

// Auto-detect dictionary if not specified
if ($config['dictionary'] === null) {
    $defaultDict = __DIR__ . '/dictionary/sql.dict';
    if (file_exists($defaultDict)) {
        $config['dictionary'] = $defaultDict;
    }
}

/**
 * Load dictionary tokens from an AFL/libFuzzer-compatible dictionary file.
 * Each line is a quoted token (quotes stripped) or a bare token.
 * Lines starting with # and blank lines are skipped.
 *
 * @param string $path Path to dictionary file
 * @return array<string> Parsed tokens
 */
function loadDictionary(string $path): array
{
    $tokens = [];
    $lines = file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    if ($lines === false) {
        echo "Warning: Could not read dictionary file: {$path}\n";
        return [];
    }
    foreach ($lines as $line) {
        $line = trim($line);
        if ($line === '' || $line[0] === '#') {
            continue;
        }
        // Strip surrounding quotes if present (AFL dict format)
        if (strlen($line) >= 2 && $line[0] === '"' && $line[-1] === '"') {
            $line = substr($line, 1, -1);
        }
        // Handle key=value format (e.g., "kw_select=\"SELECT\"")
        if (str_contains($line, '=')) {
            $line = substr($line, strpos($line, '=') + 1);
            if (strlen($line) >= 2 && $line[0] === '"' && $line[-1] === '"') {
                $line = substr($line, 1, -1);
            }
        }
        if ($line !== '') {
            $tokens[] = $line;
        }
    }
    return $tokens;
}

// Initialize report
$report = new SarifReport();
$runIndex = 0; // Use the default run created by constructor

echo "Starting fuzzer with {$config['iterations']} iterations...\n";

try {
    // Initialize harness
    $harness = new FuzzHarness($config['dsn'], $config['user'], $config['password']);
    
    // Load dictionary tokens if available
    if ($config['dictionary'] !== null) {
        $tokens = loadDictionary($config['dictionary']);
        $harness->dictionaryTokens = $tokens;
        echo "Loaded " . count($tokens) . " dictionary tokens from {$config['dictionary']}\n";
    }

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

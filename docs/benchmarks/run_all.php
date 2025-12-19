<?php
// docs/benchmarks/run_all.php
// Simple benchmark driver to run perf_firebird.php and perf_pdo_firebird.php
// against multiple Firebird engines.
//
// Usage (inside php-firebird repo root or in php*-dev container):
//   php docs/benchmarks/run_all.php
//
// This script does not modify perf_firebird.php or perf_pdo_firebird.php.
// It spawns separate PHP processes with appropriate FB_* environment
// variables so that each benchmark run targets a specific engine.

$php = PHP_BINARY;
$benchDir = __DIR__;

$engines = [
    'fb25' => [
        'label' => 'Firebird 2.5',
        'dsn'   => 'firebird:dbname=localhost/3050:/firebird/data/test.fdb;charset=UTF8',
        'host'  => 'localhost/3050:/firebird/data/test.fdb',
    ],
    'fb30' => [
        'label' => 'Firebird 3.0',
        'dsn'   => 'firebird:dbname=localhost/3051:/firebird/data/test.fdb;charset=UTF8',
        'host'  => 'localhost/3051:/firebird/data/test.fdb',
    ],
    'fb40' => [
        'label' => 'Firebird 4.0',
        'dsn'   => 'firebird:dbname=localhost/3052:/firebird/data/test.fdb;charset=UTF8',
        'host'  => 'localhost/3052:/firebird/data/test.fdb',
    ],
    'fb50' => [
        'label' => 'Firebird 5.0',
        'dsn'   => 'firebird:dbname=localhost/3053:/firebird/data/test.fdb;charset=UTF8',
        'host'  => 'localhost/3053:/firebird/data/test.fdb',
    ],
];

$drivers = [
    'firebird'     => 'perf_firebird.php',
    'pdo_firebird' => 'perf_pdo_firebird.php',
];

function run_child(string $label, string $php, string $script, string $cwd, array $envOverrides): int
{
    $cmd = [$php, $script];

    $descriptors = [
        0 => ['pipe', 'r'],
        1 => ['pipe', 'w'],
        2 => ['pipe', 'w'],
    ];

    $env = array_merge($_ENV, getenv(), $envOverrides);

    $proc = proc_open($cmd, $descriptors, $pipes, $cwd, $env);
    if (!is_resource($proc)) {
        fwrite(STDERR, "[$label] Failed to start child process for $script\n");
        return 1;
    }

    fclose($pipes[0]);

    $stdout = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);

    fclose($pipes[1]);
    fclose($pipes[2]);

    $exitCode = proc_close($proc);

    printf("===== %s =====\n", $label);
    if ($stdout !== '') {
        fwrite(STDOUT, rtrim($stdout) . "\n");
    }
    if ($stderr !== '') {
        fwrite(STDERR, "[child stderr]\n" . rtrim($stderr) . "\n");
    }
    printf("===== %s (exit code %d) =====\n\n", $label, $exitCode);

    return $exitCode;
}

$overallExit = 0;

foreach ($engines as $key => $engine) {
    foreach ($drivers as $driverKey => $script) {
        $label = sprintf('%s / %s', $engine['label'], $driverKey);

        $env = [
            'FB_DSN'      => $engine['dsn'],
            'FB_HOST'     => $engine['host'],
            'FB_USER'     => getenv('FB_USER') ?: 'SYSDBA',
            'FB_PASSWORD' => getenv('FB_PASSWORD') ?: 'masterkey',
            'FB_CHARSET'  => getenv('FB_CHARSET') ?: 'UTF8',
        ];

        $exit = run_child($label, $php, $script, $benchDir, $env);
        if ($exit !== 0 && $overallExit === 0) {
            $overallExit = $exit;
        }
    }
}

exit($overallExit);

<?php
// satware-docs/benchmarks/init_perf_table.php
// Initialize the perf_test table in the configured Firebird database.
//
// Usage:
//   FB_HOST='localhost/3051:/firebird/data/test.fdb' \
//   FB_DSN='firebird:dbname=localhost/3051:/firebird/data/test.fdb;charset=UTF8' \
//   FB_USER='SYSDBA' FB_PASSWORD='masterkey' FB_CHARSET='UTF8' \
//   php satware-docs/benchmarks/init_perf_table.php
//
// This uses the classic interbase extension and config.php env overrides.

if (!extension_loaded('interbase')) {
    fwrite(STDERR, "The 'interbase' extension is not loaded.\\n");
    exit(1);
}

$config = require __DIR__ . '/config.php';

$link = @ibase_connect(
    $config['host'],
    $config['user'],
    $config['password'],
    $config['charset']
);

if (!$link) {
    fwrite(STDERR, "Failed to connect to Firebird using host '{$config['host']}'.\\n");
    exit(1);
}

// Create perf_test table if it does not exist, then seed a single row.
@ibase_query($link, 'CREATE TABLE perf_test (id INTEGER PRIMARY KEY, payload VARCHAR(100))');
@ibase_query($link, 'DELETE FROM perf_test');
@ibase_query($link, "INSERT INTO perf_test(id, payload) VALUES (1, 'test row')");
@ibase_commit($link);

fwrite(STDOUT, "perf_test initialized on {$config['host']}\\n");

<?php
// docs/benchmarks/dpb_probe.php
// Probe DPB/attach behaviour for the php-firebird extension
// against a single Firebird engine.
//
// Usage (from repo root or php*-dev container):
//   php docs/benchmarks/dpb_probe.php
//
// Configuration is taken from config.php, which in turn honours:
//   FB_DSN, FB_HOST, FB_USER, FB_PASSWORD, FB_CHARSET
//
// Typical Docker mappings (see docker-compose.yml):
//   Firebird 2.5: FB_HOST=localhost/3050:/firebird/data/test.fdb
//   Firebird 3.0: FB_HOST=localhost/3051:/firebird/data/test.fdb
//   Firebird 4.0: FB_HOST=localhost/3052:/firebird/data/test.fdb
//   Firebird 5.0: FB_HOST=localhost/3053:/firebird/data/test.fdb

if (!extension_loaded('firebird')) {
    fwrite(STDERR, "The 'firebird' extension is not loaded.\n");
    exit(1);
}

$config = require __DIR__ . '/config.php';

$host     = $config['host'];
$user     = $config['user'];
$password = $config['password'];
$charset  = $config['charset'];

printf("== DPB probe using fbird_connect() ==\n");
printf("Host     : %s\n", $host);
printf("User     : %s\n", $user);
printf("Charset  : %s\n", $charset);

if (function_exists('fbird_get_client_version')) {
    $clientMajor = function_exists('fbird_get_client_major_version') ? fbird_get_client_major_version() : -1;
    $clientMinor = function_exists('fbird_get_client_minor_version') ? fbird_get_client_minor_version() : -1;
    $clientCombined = fbird_get_client_version();
    printf("Client   : major=%d minor=%d combined=%.1f\n", $clientMajor, $clientMinor, $clientCombined);
}

$link = @fbird_connect($host, $user, $password, $charset);

if ($link === false) {
    $errcode = function_exists('fbird_errcode') ? fbird_errcode() : 0;
    $errmsg  = function_exists('fbird_errmsg') ? fbird_errmsg() : 'unknown error';

    printf("Result   : ATTACH FAILED\n");
    printf("ErrCode  : %s\n", $errcode === false ? 'false' : (string) $errcode);
    printf("ErrMsg   : %s\n", $errmsg);
    exit(1);
}

printf("Result   : ATTACH OK\n");

// Try to query engine version where supported (Firebird 2.5+).
$version = null;
$verRes = @fbird_query($link, "SELECT rdb\$get_context('SYSTEM', 'ENGINE_VERSION') FROM RDB\$DATABASE");
if ($verRes !== false) {
    $row = fbird_fetch_row($verRes);
    if ($row !== false && isset($row[0])) {
        $version = $row[0];
    }
    fbird_free_result($verRes);
}

if ($version !== null) {
    printf("Engine   : %s\n", $version);
} else {
    printf("Engine   : (could not determine via rdb\$get_context)\n");
}

fbird_close($link);

printf("Done.\n");

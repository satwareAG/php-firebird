--TEST--
fbird_server_info() basic and expanded constants
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

$service = fbird_service_attach($host, $user, $pass);

// Standard info
$version = fbird_server_info($service, FBIRD_SVC_SERVER_VERSION);
var_dump(is_string($version) && strlen($version) > 0);

$impl = fbird_server_info($service, FBIRD_SVC_IMPLEMENTATION);
var_dump(is_string($impl) && strlen($impl) > 0);

$env = fbird_server_info($service, FBIRD_SVC_GET_ENV);
var_dump(is_string($env));

$dbpath = fbird_server_info($service, FBIRD_SVC_USER_DBPATH);
var_dump(is_string($dbpath));

// New constants coverage
if (defined('FBIRD_SVC_GET_ENV_LOCK')) {
    $lock = fbird_server_info($service, FBIRD_SVC_GET_ENV_LOCK);
    var_dump(is_string($lock));
} else {
    echo "FBIRD_SVC_GET_ENV_LOCK not defined\n";
}

if (defined('FBIRD_SVC_GET_ENV_MSG')) {
    $msg = fbird_server_info($service, FBIRD_SVC_GET_ENV_MSG);
    var_dump(is_string($msg));
} else {
    echo "FBIRD_SVC_GET_ENV_MSG not defined\n";
}

fbird_service_detach($service);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

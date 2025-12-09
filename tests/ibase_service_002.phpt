--TEST--
ibase_server_info() basic and expanded constants
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

$service = ibase_service_attach($host, $user, $pass);

// Standard info
$version = ibase_server_info($service, FBIRD_SVC_SERVER_VERSION);
var_dump(is_string($version) && strlen($version) > 0);

$impl = ibase_server_info($service, FBIRD_SVC_IMPLEMENTATION);
var_dump(is_string($impl) && strlen($impl) > 0);

$env = ibase_server_info($service, FBIRD_SVC_GET_ENV);
var_dump(is_string($env));

$dbpath = ibase_server_info($service, FBIRD_SVC_USER_DBPATH);
var_dump(is_string($dbpath));

// New constants coverage
if (defined('FBIRD_SVC_GET_ENV_LOCK')) {
    $lock = ibase_server_info($service, FBIRD_SVC_GET_ENV_LOCK);
    var_dump(is_string($lock));
} else {
    echo "FBIRD_SVC_GET_ENV_LOCK not defined\n";
}

if (defined('FBIRD_SVC_GET_ENV_MSG')) {
    $msg = ibase_server_info($service, FBIRD_SVC_GET_ENV_MSG);
    var_dump(is_string($msg));
} else {
    echo "FBIRD_SVC_GET_ENV_MSG not defined\n";
}

ibase_service_detach($service);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)

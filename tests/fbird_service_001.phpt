--TEST--
fbird_service_attach() specific error messages
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

// 1. Successful attach
$service = fbird_service_attach($host, $user, $pass);
var_dump(is_resource($service) || $service instanceof \Firebird\Service);
fbird_service_detach($service);

// 2. Auth failure
// Suppress warning to handle error manually
$service = @fbird_service_attach($host, $user, 'wrongpassword');
if ($service === false) {
    echo "Auth failed as expected\n";
    $msg = fbird_errmsg();
    // Keywords common across Firebird versions for auth failure
    // FB 2.5: "Login incorrect"
    // FB 3.0+: "Your user name and password are not defined..."
    // We check for "password", "login", "user", or "authentication"
    $is_auth_error = stripos($msg, 'password') !== false
                  || stripos($msg, 'login') !== false
                  || stripos($msg, 'user') !== false
                  || stripos($msg, 'authentication') !== false;

    if (!$is_auth_error) {
        echo "Unexpected auth error message: $msg\n";
    }
    var_dump($is_auth_error);
}

// 3. Host failure
$service = @fbird_service_attach('nonexistent_host', $user, $pass);
if ($service === false) {
    echo "Host failed as expected\n";
    $msg = fbird_errmsg();
    // Keywords for network/host failure
    // "Unable to complete network request", "Unknown host", "Connection refused"
    $is_host_error = stripos($msg, 'network') !== false
                  || stripos($msg, 'host') !== false
                  || stripos($msg, 'connection') !== false;

    if (!$is_host_error) {
        echo "Unexpected host error message: $msg\n";
    }
    var_dump($is_host_error);
}

?>
--EXPECTF--
bool(true)
Auth failed as expected
bool(true)
Host failed as expected
bool(true)

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

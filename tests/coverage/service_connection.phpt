--TEST--
Service API connection cycle coverage
--SKIPIF--
<?php
include(__DIR__ . "/../skipif.inc");
?>
--FILE--
<?php
/**
 * Coverage test for fbird_service_attach() and fbird_service_detach()
 *
 * Targets fbird_service.c code paths:
 * - fbird_service_attach(): Connection with credentials
 * - fbird_service_detach(): Resource cleanup
 * - _php_fbird_free_service(): Destructor (NULL checks, fork guards)
 * - Multiple connect/disconnect cycles
 */
require(__DIR__ . "/../firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

echo "=== Test 1: Basic connection cycle ===\n";
$service = fbird_service_attach($host, $user, $pass);
var_dump(is_resource($service) || $service instanceof \Firebird\Service);
$detach_result = fbird_service_detach($service);
var_dump($detach_result);

echo "=== Test 2: Multiple connect/disconnect cycles ===\n";
for ($i = 1; $i <= 3; $i++) {
    $svc = fbird_service_attach($host, $user, $pass);
    $is_valid = is_resource($svc) || $svc instanceof \Firebird\Service;
    echo "Cycle $i connect: " . ($is_valid ? "OK" : "FAIL") . "\n";
    fbird_service_detach($svc);
}
echo "Multiple cycles completed\n";

echo "=== Test 3: Connection without explicit host (localhost) ===\n";
// Test with empty host - should use localhost
$service_local = @fbird_service_attach('', $user, $pass);
if ($service_local !== false) {
    var_dump(is_resource($service_local) || $service_local instanceof \Firebird\Service);
    fbird_service_detach($service_local);
} else {
    // Some configurations may fail - acceptable
    echo "Local connection not available (expected in some configs)\n";
}

echo "=== Test 4: Connection with INI defaults (no user/pass args) ===\n";
// This tests the INI fallback code path in fbird_service_attach
// The function falls back to fbird.default_user and fbird.default_password
$service_ini = @fbird_service_attach($host);
if ($service_ini !== false) {
    var_dump(is_resource($service_ini) || $service_ini instanceof \Firebird\Service);
    fbird_service_detach($service_ini);
    echo "INI defaults used successfully\n";
} else {
    echo "INI defaults connection: " . (fbird_errmsg() ? "auth error (expected without INI)" : "failed") . "\n";
}

echo "=== Test 5: Detach on already-detached service (idempotent) ===\n";
$service = fbird_service_attach($host, $user, $pass);
fbird_service_detach($service);
// Second detach should not crash - tests destructor NULL guard
$result = @fbird_service_detach($service);
echo "Double detach handled: OK\n";

echo "=== Test 6: Server info query after connection ===\n";
$service = fbird_service_attach($host, $user, $pass);
$version = fbird_server_info($service, FBIRD_SVC_SERVER_VERSION);
var_dump(is_string($version) && strlen($version) > 0);
$impl = fbird_server_info($service, FBIRD_SVC_IMPLEMENTATION);
var_dump(is_string($impl) && strlen($impl) > 0);
fbird_service_detach($service);

echo "=== Test 7: Multiple services simultaneously ===\n";
$svc1 = fbird_service_attach($host, $user, $pass);
$svc2 = fbird_service_attach($host, $user, $pass);
var_dump(is_resource($svc1) || $svc1 instanceof \Firebird\Service);
var_dump(is_resource($svc2) || $svc2 instanceof \Firebird\Service);
// They should be different resources
var_dump($svc1 !== $svc2);
fbird_service_detach($svc1);
fbird_service_detach($svc2);

echo "=== Done ===\n";
?>
--EXPECTF--
=== Test 1: Basic connection cycle ===
bool(true)
bool(true)
=== Test 2: Multiple connect/disconnect cycles ===
Cycle 1 connect: OK
Cycle 2 connect: OK
Cycle 3 connect: OK
Multiple cycles completed
=== Test 3: Connection without explicit host (localhost) ===
%A
=== Test 4: Connection with INI defaults (no user/pass args) ===
%A
=== Test 5: Detach on already-detached service (idempotent) ===
Double detach handled: OK
=== Test 6: Server info query after connection ===
bool(true)
bool(true)
=== Test 7: Multiple services simultaneously ===
bool(true)
bool(true)
bool(true)
=== Done ===

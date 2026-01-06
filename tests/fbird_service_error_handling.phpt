--TEST--
Service API error handling - graceful failure after resource invalidation (Issue #64)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

echo "=== Test 1: Use service after detach should return false or throw, not crash ===\n";

$service = @fbird_service_attach($host, $user, $pass);
if (!$service) {
    die("skip Could not connect to service manager\n");
}

// Detach the service
fbird_service_detach($service);

// Using detached service should return false or throw TypeError, not SIGSEGV crash (Issue #64)
// The @ operator suppresses warnings but not TypeErrors, so we use try/catch
$test1_passed = false;
try {
    $result = @fbird_backup($service, '/tmp/test.fdb', '/tmp/test_backup.fbk');
    $test1_passed = ($result === false);
} catch (TypeError $e) {
    // TypeError means resource validation caught invalid handle - this is safe behavior
    $test1_passed = true;
}
var_dump($test1_passed);

$test2_passed = false;
try {
    $result = @fbird_server_info($service, FBIRD_SVC_SERVER_VERSION);
    $test2_passed = ($result === false);
} catch (TypeError $e) {
    $test2_passed = true;
}
var_dump($test2_passed);

echo "=== Test 2: Multiple operations after detach should all return false or throw ===\n";

$service2 = @fbird_service_attach($host, $user, $pass);
if (!$service2) {
    die("skip Could not connect to service manager\n");
}

fbird_service_detach($service2);

// All these should return false or throw TypeError (not crash)
$all_safe = true;
$operations = [
    function($svc) { return @fbird_backup($svc, '/tmp/test.fdb', '/tmp/test.fbk'); },
    function($svc) { return @fbird_restore($svc, '/tmp/test.fbk', '/tmp/test_restore.fdb'); },
    function($svc) { return @fbird_server_info($svc, FBIRD_SVC_SERVER_VERSION); },
];

foreach ($operations as $op) {
    try {
        $result = $op($service2);
        if ($result !== false) {
            $all_safe = false;
            break;
        }
    } catch (TypeError $e) {
        // TypeError is safe - means resource validation caught invalid handle
        continue;
    }
}
var_dump($all_safe);

echo "=== Issue #64 validation complete ===\n";
?>
--EXPECT--
=== Test 1: Use service after detach should return false or throw, not crash ===
bool(true)
bool(true)
=== Test 2: Multiple operations after detach should all return false or throw ===
bool(true)
=== Issue #64 validation complete ===

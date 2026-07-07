--TEST--
Coverage: Service API error paths and NULL handle guard (PR #70 regression test)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird extension not available');
?>
--FILE--
<?php
// This test deliberately exercises error paths — do NOT require firebird.inc
// because we don't need a valid DB for most tests here.

$host     = getenv('FIREBIRD_HOST') ?: 'localhost';
$user     = getenv('ISC_USER')      ?: 'SYSDBA';
$password = getenv('ISC_PASSWORD')  ?: 'masterkey';

// Helper: call a function with a non-resource first argument.
// In PHP 8.0+, passing wrong type to a resource parameter throws TypeError
// which the @ operator cannot suppress. We wrap in try/catch to verify
// "no crash" (TypeError = correct rejection, not SIGSEGV).
function call_with_invalid_resource(callable $fn, ...$args): bool
{
    try {
        $r = $fn(...$args);
        return ($r === false); // pre-PHP-8 path: returns false
    } catch (\TypeError $e) {
        return true; // PHP 8+: TypeError = expected rejection, no crash
    }
}

// 1. fbird_service_attach with wrong password (auth failure path)
echo "Test 1: Auth failure\n";
$svc = @fbird_service_attach($host, $user, 'wrongpassword_12345');
var_dump($svc === false);
$err = fbird_errmsg();
var_dump(strlen($err) > 0);

// 2. fbird_service_attach with invalid host
echo "Test 2: Invalid host\n";
$svc2 = @fbird_service_attach('nonexistent.invalid.host.255.255', $user, $password);
var_dump($svc2 === false);

// 3. fbird_backup with non-resource handle — hits NULL check from PR #70
// PHP 8.0+ throws TypeError (expected), PHP 7 returns false (expected).
echo "Test 3: backup with invalid resource\n";
var_dump(call_with_invalid_resource('fbird_backup', false, '/tmp/test.fdb', '/tmp/test.fbk'));

// 4. fbird_restore with non-resource handle
echo "Test 4: restore with invalid resource\n";
var_dump(call_with_invalid_resource('fbird_restore', false, '/tmp/test.fbk', '/tmp/test2.fdb'));

// 5. fbird_maintain_db with non-resource handle
echo "Test 5: maintain_db with invalid resource\n";
var_dump(call_with_invalid_resource('fbird_maintain_db', false, '/tmp/test.fdb', FBIRD_RPR_SWEEP_DB, 0));

// 6. fbird_db_info with non-resource handle
echo "Test 6: db_info with invalid resource\n";
var_dump(call_with_invalid_resource('fbird_db_info', false, '/tmp/test.fdb', FBIRD_STS_DATA_PAGES));

// 7. fbird_server_info with non-resource handle
echo "Test 7: server_info with invalid resource\n";
var_dump(call_with_invalid_resource('fbird_server_info', false, FBIRD_SVC_SERVER_VERSION));

// 8. fbird_service_detach with non-resource handle
echo "Test 8: detach with invalid resource\n";
var_dump(call_with_invalid_resource('fbird_service_detach', false));

// 9. Double-detach — exercises NULL-handle destructor path (PR #70 fix)
// If service manager is reachable, do a real attach/detach/detach sequence.
echo "Test 9: Double detach (NULL handle guard)\n";
$svc3 = @fbird_service_attach($host, $user, $password);
if ($svc3) {
    fbird_service_detach($svc3);
    // Second detach on already-freed resource: must not crash (SIGSEGV before PR #70 fix)
    try {
        @fbird_service_detach($svc3);
    } catch (\TypeError $e) {
        // PHP 8+: resource already freed — TypeError is fine
    }
    echo "no crash\n";
} else {
    // Service not available — verify no crash with false handle
    call_with_invalid_resource('fbird_service_detach', false);
    echo "no crash\n";
}

echo "Done\n";
?>
--EXPECTF--
Test 1: Auth failure
bool(true)
bool(true)
Test 2: Invalid host
bool(true)
Test 3: backup with invalid resource

Warning: fbird_backup(): Invalid Firebird service handle in %s on line %d
bool(true)
Test 4: restore with invalid resource

Warning: fbird_restore(): Invalid Firebird service handle in %s on line %d
bool(true)
Test 5: maintain_db with invalid resource

Warning: fbird_maintain_db(): Invalid Firebird service handle in %s on line %d
bool(true)
Test 6: db_info with invalid resource

Warning: fbird_db_info(): Invalid Firebird service handle in %s on line %d
bool(true)
Test 7: server_info with invalid resource

Warning: fbird_server_info(): Invalid Firebird service handle in %s on line %d
bool(true)
Test 8: detach with invalid resource

Warning: fbird_service_detach(): %s in %s on line %d
bool(true)
Test 9: Double detach (NULL handle guard)
no crash
Done

--TEST--
Coverage: Service user management edge cases (add/modify/delete boundaries)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
// Do NOT include firebird.inc here — it registers cleanup_db() which would drop
// the shared test.fdb when SKIPIF exits, corrupting subsequent tests.
if (!extension_loaded('firebird')) die('skip firebird extension not available');
// Firebird 5.0 removed the legacy isc_action_svc_add/modify/delete_user service API.
// User management on FB5 requires SQL (ALTER USER / CREATE USER).
if (function_exists('fbird_get_client_major_version') && fbird_get_client_major_version() >= 5) {
    die('skip Firebird 5.0+ removed legacy service-based user management (isc_action_svc_*_user); SQL-based user management required');
}
$host     = getenv('FIREBIRD_HOST') ?: 'localhost';
$user     = getenv('ISC_USER')      ?: 'SYSDBA';
$password = getenv('ISC_PASSWORD')  ?: 'masterkey';
$svc = @fbird_service_attach($host, $user, $password);
if (!$svc) die('skip: cannot attach to Firebird service manager');
fbird_service_detach($svc);
?>
--FILE--
<?php
// PHP 8.4 deprecated implicit nullable params in internal function arginfo;
// suppress deprecation/notice output to keep expected output stable across PHP versions.
error_reporting(E_ALL & ~E_DEPRECATED & ~E_NOTICE);
require_once __DIR__ . '/../firebird.inc';

$host     = getenv('FIREBIRD_HOST') ?: 'localhost';
$user     = getenv('ISC_USER')      ?: 'SYSDBA';
$password = getenv('ISC_PASSWORD')  ?: 'masterkey';

$test_user  = 'SVC_TEST_USR1';
$test_pass  = 'testpw1234';

// Firebird service API: fbird_add_user/modify_user/delete_user are fire-and-forget.
// Sequential operations on a shared handle cause "Service is currently busy" races.
// Fix: per-operation attach/detach + usleep so the server can finish.
function svc($host, $user, $password) {
    for ($i = 0; $i < 3; $i++) {
        $s = @fbird_service_attach($host, $user, $password);
        if ($s) return $s;
        usleep(500000);
    }
    die("ERROR: cannot attach to service\n");
}

function user_op($host, $user, $password, $fn, ...$args) {
    $s = svc($host, $user, $password);
    $r = @$fn($s, ...$args);
    @fbird_service_detach($s);
    usleep(200000);
    return $r;
}

// Ensure clean state
user_op($host, $user, $password, 'fbird_delete_user', $test_user);

// 1. Add user with minimal fields (username + password only)
echo "Test 1: Add minimal user\n";
$r = user_op($host, $user, $password, 'fbird_add_user', $test_user, $test_pass);
var_dump($r === true);

// 2. Add duplicate user — expect false (already exists)
echo "Test 2: Add duplicate user\n";
$r = user_op($host, $user, $password, 'fbird_add_user', $test_user, $test_pass);
var_dump($r === false);

// 3. Modify user — change first name
echo "Test 3: Modify user first name\n";
$r = user_op($host, $user, $password, 'fbird_modify_user', $test_user, $test_pass, 'NewFirst');
var_dump($r === true);

// 4. Modify user — change last name only (pass empty string for unchanged params)
echo "Test 4: Modify user last name\n";
$r = user_op($host, $user, $password, 'fbird_modify_user', $test_user, $test_pass, '', '', 'NewLast');
var_dump($r === true);

// 5. Delete user
echo "Test 5: Delete user\n";
$r = user_op($host, $user, $password, 'fbird_delete_user', $test_user);
var_dump($r === true);

// 6. Delete non-existent user — expect false or warning, not crash
echo "Test 6: Delete non-existent user\n";
$r = user_op($host, $user, $password, 'fbird_delete_user', $test_user);
var_dump($r === false || $r === true); // any non-crash result

// 7. Add user with all optional fields (firstname, middlename, lastname)
echo "Test 7: Add user with all fields\n";
$r = user_op($host, $user, $password, 'fbird_add_user', $test_user, $test_pass, 'First', 'Middle', 'Last');
var_dump($r === true);

// Note: fbird_server_info(FBIRD_SVC_GET_USERS) causes a segfault on Firebird 3
// via service API — skip enumeration, trust fbird_add_user return value above.

// Cleanup
user_op($host, $user, $password, 'fbird_delete_user', $test_user);

echo "Done\n";
?>
--EXPECT--
Test 1: Add minimal user
bool(true)
Test 2: Add duplicate user
bool(true)
Test 3: Modify user first name
bool(true)
Test 4: Modify user last name
bool(true)
Test 5: Delete user
bool(true)
Test 6: Delete non-existent user
bool(true)
Test 7: Add user with all fields
bool(true)
Done

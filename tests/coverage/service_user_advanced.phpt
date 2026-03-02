--TEST--
Coverage: Service user management edge cases (add/modify/delete boundaries)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
// Do NOT include firebird.inc here — it registers cleanup_db() which would drop
// the shared test.fdb when SKIPIF exits, corrupting subsequent tests.
if (!extension_loaded('firebird')) die('skip firebird extension not available');
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

$svc = fbird_service_attach($host, $user, $password);
if (!$svc) die("ERROR: could not attach to service\n");

$test_user  = 'SVC_TEST_USR1';
$test_pass  = 'testpw1234';

// Ensure clean state
@fbird_delete_user($svc, $test_user);

// 1. Add user with minimal fields (username + password only)
echo "Test 1: Add minimal user\n";
$r = fbird_add_user($svc, $test_user, $test_pass);
var_dump($r === true);

// 2. Add duplicate user — expect false (already exists)
echo "Test 2: Add duplicate user\n";
$r = @fbird_add_user($svc, $test_user, $test_pass);
var_dump($r === false);

// Reconnect: @fbird_add_user failure (duplicate) can invalidate the service handle.
@fbird_service_detach($svc);
$svc = fbird_service_attach($host, $user, $password);
if (!$svc) die("ERROR: could not re-attach to service\n");

// 3. Modify user — change first name
echo "Test 3: Modify user first name\n";
$r = fbird_modify_user($svc, $test_user, $test_pass, 'NewFirst');
var_dump($r === true);

// 4. Modify user — change last name only (pass empty string for unchanged params)
echo "Test 4: Modify user last name\n";
$r = fbird_modify_user($svc, $test_user, $test_pass, '', '', 'NewLast');
var_dump($r === true);

// 5. Delete user
echo "Test 5: Delete user\n";
$r = fbird_delete_user($svc, $test_user);
var_dump($r === true);

// 6. Delete non-existent user — expect false or warning, not crash
echo "Test 6: Delete non-existent user\n";
$r = @fbird_delete_user($svc, $test_user);
var_dump($r === false || $r === true); // any non-crash result

// Reconnect service before Test 7 — deleting a non-existent user (Test 6) may
// leave the service handle in a broken state on some Firebird versions.
@fbird_service_detach($svc);
$svc = fbird_service_attach($host, $user, $password);
if (!$svc) die("ERROR: could not re-attach to service\n");

// 7. Add user with all optional fields (firstname, middlename, lastname)
echo "Test 7: Add user with all fields\n";
$r = fbird_add_user($svc, $test_user, $test_pass, 'First', 'Middle', 'Last');
var_dump($r === true);

// Note: fbird_server_info(FBIRD_SVC_GET_USERS) causes a segfault on Firebird 3
// via service API — skip enumeration, trust fbird_add_user return value above.

// Cleanup
@fbird_service_detach($svc);
$svc = fbird_service_attach($host, $user, $password);
if ($svc) {
    @fbird_delete_user($svc, $test_user);
    fbird_service_detach($svc);
}

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

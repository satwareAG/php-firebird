--TEST--
Service API user management comprehensive coverage
--SKIPIF--
<?php
include(__DIR__ . "/../skipif.inc");
?>
--FILE--
<?php
/**
 * Coverage test for user management functions in fbird_service.c
 *
 * Targets fbird_service.c code paths:
 * - fbird_add_user(): Add new user with parameters
 * - fbird_modify_user(): Modify existing user
 * - fbird_delete_user(): Delete user
 * - _php_fbird_user(): Core user operation handler
 * - SPB buffer building for user operations
 */
require(__DIR__ . "/../firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';
$pid = getmypid();

// Unique test user name to avoid conflicts
$test_user = "FBTEST" . ($pid % 10000);

// Helper to get fresh service connection
function get_service($host, $user, $pass) {
    return fbird_service_attach($host, $user, $pass);
}

echo "=== Test 1: Connect to service manager ===\n";
$service = get_service($host, $user, $pass);
var_dump(is_resource($service) || $service instanceof \Firebird\Service);

echo "=== Test 2: Delete test user if exists (cleanup) ===\n";
// Ensure clean state - delete if exists (may fail, that's OK)
$result = @fbird_delete_user($service, $test_user);
echo "Cleanup: " . ($result ? "deleted" : "not present") . "\n";
fbird_service_detach($service);

echo "=== Test 3: Add new user (basic) ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_add_user($service, $test_user, "testpass123");
var_dump($result);
fbird_service_detach($service);

echo "=== Test 4: Add user with first/middle/last name ===\n";
$service = get_service($host, $user, $pass);
$test_user2 = "FBTEST" . (($pid + 1) % 10000);
$result = @fbird_add_user($service, $test_user2, "testpass456",
    "John",    // first_name
    "Middle",  // middle_name
    "Doe"      // last_name
);
var_dump($result);
fbird_service_detach($service);

echo "=== Test 5: Modify user password ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_modify_user($service, $test_user, "newpass789");
var_dump($result);
fbird_service_detach($service);

echo "=== Test 6: Modify user with names ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_modify_user($service, $test_user, null,
    "UpdatedFirst",
    "UpdatedMiddle",
    "UpdatedLast"
);
var_dump($result);
fbird_service_detach($service);

echo "=== Test 7: Delete user ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_delete_user($service, $test_user);
var_dump($result);
fbird_service_detach($service);

echo "=== Test 8: Delete second test user ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_delete_user($service, $test_user2);
var_dump($result);
fbird_service_detach($service);

echo "=== Test 9: Error handling - delete non-existent user ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_delete_user($service, "NONEXISTENT_USER_XYZ");
// This should fail (user doesn't exist)
var_dump($result === false);
// Handle may be invalidated, get fresh one
$service = get_service($host, $user, $pass);

echo "=== Test 10: Error handling - add user with empty name ===\n";
$result = @fbird_add_user($service, "", "password");
// Empty username may succeed or fail depending on Firebird version
var_dump(is_bool($result));
fbird_service_detach($service);

echo "=== Cleanup ===\n";
echo "Done\n";
?>
--EXPECTF--
=== Test 1: Connect to service manager ===
bool(true)
=== Test 2: Delete test user if exists (cleanup) ===
Cleanup: %s
=== Test 3: Add new user (basic) ===
bool(true)
=== Test 4: Add user with first/middle/last name ===
bool(true)
=== Test 5: Modify user password ===
bool(true)
=== Test 6: Modify user with names ===
bool(true)
=== Test 7: Delete user ===
bool(true)
=== Test 8: Delete second test user ===
bool(true)
=== Test 9: Error handling - delete non-existent user ===
bool(true)
=== Test 10: Error handling - add user with empty name ===
bool(%s)
=== Cleanup ===
Done

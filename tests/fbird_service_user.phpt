--TEST--
fbird_service_user: Add, Modify, Delete, and List Users
--SKIPIF--
<?php
include("skipif.inc");
// Firebird 5.0 removed the legacy isc_action_svc_add/modify/delete_user service API.
// User management on FB5 requires SQL (CREATE USER / ALTER USER / DROP USER).
if (function_exists('fbird_get_client_major_version') && fbird_get_client_major_version() >= 5) {
    die('skip Firebird 5.0+ removed legacy service-based user management; SQL-based user management required');
}
?>
--FILE--
<?php
require("firebird.inc");

$new_user = 'PHP_TEST_USR';
$new_pass = 'pw123456';
$mod_pass = 'pw654321';

// 1. Attach
$service = fbird_service_attach($host, $user, $password);
if (!$service) die("Skip: Check service connection");

// Cleanup (just in case)
@fbird_delete_user($service, $new_user);

// 2. Add User
echo "--- Add User ---\n";
$res = fbird_add_user($service, $new_user, $new_pass, 'Test', 'Middle', 'User');
var_dump($res);

// 3. Verify User Exists
echo "--- Verify User ---\n";
$users = fbird_server_info($service, FBIRD_SVC_GET_USERS);
$found = false;
foreach ($users as $u) {
    if ($u['user_name'] === $new_user) {
        $found = true;
        var_dump($u['first_name']);
        break;
    }
}
var_dump($found);

// 4. Modify User
echo "--- Modify User ---\n";
$res = fbird_modify_user($service, $new_user, $mod_pass, 'TestMod');
var_dump($res);

// 5. Verify Modification
$users = fbird_server_info($service, FBIRD_SVC_GET_USERS);
foreach ($users as $u) {
    if ($u['user_name'] === $new_user) {
        var_dump($u['first_name']);
        break;
    }
}

// 6. Delete User
echo "--- Delete User ---\n";
$res = fbird_delete_user($service, $new_user);
var_dump($res);

// 7. Verify Deletion
$users = fbird_server_info($service, FBIRD_SVC_GET_USERS);
$found = false;
foreach ($users as $u) {
    if ($u['user_name'] === $new_user) {
        $found = true;
        break;
    }
}
var_dump($found);

fbird_service_detach($service);
?>
--EXPECT--
--- Add User ---
bool(true)
--- Verify User ---
string(4) "Test"
bool(true)
--- Modify User ---
bool(true)
string(7) "TestMod"
--- Delete User ---
bool(true)
bool(false)

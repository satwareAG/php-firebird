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

// Firebird service API: fbird_add_user / fbird_modify_user / fbird_delete_user
// are fire-and-forget (fbsvc_start returns immediately). Sequential operations
// on a shared handle cause "Service is currently busy" races.
// Fix: per-operation attach/detach + usleep so the server can finish.
function svc($host, $user, $password) {
    for ($i = 0; $i < 3; $i++) {
        $s = @fbird_service_attach($host, $user, $password);
        if ($s) return $s;
        usleep(500000);
    }
    die("ERROR: cannot attach to service\n");
}

// Cleanup (just in case) - use a fresh handle
$s = svc($host, $user, $password);
@fbird_delete_user($s, $new_user);
fbird_service_detach($s);
usleep(200000);

// 2. Add User
echo "--- Add User ---\n";
$s = svc($host, $user, $password);
$res = fbird_add_user($s, $new_user, $new_pass, 'Test', 'Middle', 'User');
fbird_service_detach($s);
usleep(200000);
var_dump($res);

// 3. Verify User Exists
echo "--- Verify User ---\n";
$s = svc($host, $user, $password);
$users = fbird_server_info($s, FBIRD_SVC_GET_USERS);
fbird_service_detach($s);
usleep(200000);
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
$s = svc($host, $user, $password);
$res = fbird_modify_user($s, $new_user, $mod_pass, 'TestMod');
fbird_service_detach($s);
usleep(200000);
var_dump($res);

// 5. Verify Modification
$s = svc($host, $user, $password);
$users = fbird_server_info($s, FBIRD_SVC_GET_USERS);
fbird_service_detach($s);
usleep(200000);
foreach ($users as $u) {
    if ($u['user_name'] === $new_user) {
        var_dump($u['first_name']);
        break;
    }
}

// 6. Delete User
echo "--- Delete User ---\n";
$s = svc($host, $user, $password);
$res = fbird_delete_user($s, $new_user);
fbird_service_detach($s);
usleep(200000);
var_dump($res);

// 7. Verify Deletion
$s = svc($host, $user, $password);
$users = fbird_server_info($s, FBIRD_SVC_GET_USERS);
fbird_service_detach($s);
$found = false;
foreach ($users as $u) {
    if ($u['user_name'] === $new_user) {
        $found = true;
        break;
    }
}
var_dump($found);
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

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

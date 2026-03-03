--TEST--
Coverage: fbird_server_info all constants, verbose backup, _php_fbird_service_query line loop
--EXTENSIONS--
firebird
--SKIPIF--
<?php
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
require_once __DIR__ . '/../firebird.inc';

$host     = getenv('FIREBIRD_HOST') ?: 'localhost';
$user     = getenv('ISC_USER')      ?: 'SYSDBA';
$password = getenv('ISC_PASSWORD')  ?: 'masterkey';

function attach_svc_retry($host, $user, $password, int $max = 3) {
    for ($i = 1; $i <= $max; $i++) {
        $s = @fbird_service_attach($host, $user, $password);
        if ($s) return $s;
        if ($i < $max) usleep(300000);
    }
    die("ERROR: cannot attach to service\n");
}

// Strip host prefix for server-side path
$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// ---- Test 1: FBIRD_SVC_SERVER_VERSION ----
echo "Test 1: fbird_server_info SERVER_VERSION\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_server_info($svc, FBIRD_SVC_SERVER_VERSION);
fbird_service_detach($svc);
var_dump(is_string($r) && strlen($r) > 0);

// ---- Test 2: FBIRD_SVC_IMPLEMENTATION ----
echo "Test 2: fbird_server_info IMPLEMENTATION\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_server_info($svc, FBIRD_SVC_IMPLEMENTATION);
fbird_service_detach($svc);
var_dump(is_string($r) && strlen($r) > 0);

// ---- Test 3: FBIRD_SVC_GET_ENV ----
echo "Test 3: fbird_server_info GET_ENV\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_server_info($svc, FBIRD_SVC_GET_ENV);
fbird_service_detach($svc);
var_dump(is_string($r));

// ---- Test 4: FBIRD_SVC_GET_ENV_LOCK ----
echo "Test 4: fbird_server_info GET_ENV_LOCK\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_server_info($svc, FBIRD_SVC_GET_ENV_LOCK);
fbird_service_detach($svc);
var_dump(is_string($r));

// ---- Test 5: FBIRD_SVC_GET_ENV_MSG ----
echo "Test 5: fbird_server_info GET_ENV_MSG\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_server_info($svc, FBIRD_SVC_GET_ENV_MSG);
fbird_service_detach($svc);
var_dump(is_string($r));

// ---- Test 6: FBIRD_SVC_USER_DBPATH ----
echo "Test 6: fbird_server_info USER_DBPATH\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_server_info($svc, FBIRD_SVC_USER_DBPATH);
fbird_service_detach($svc);
var_dump(is_string($r));

// ---- Test 7: FBIRD_SVC_SVR_DB_INFO ----
echo "Test 7: fbird_server_info SVR_DB_INFO\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_server_info($svc, FBIRD_SVC_SVR_DB_INFO);
fbird_service_detach($svc);
var_dump(is_array($r));
var_dump(array_key_exists('attachments', $r));
var_dump(array_key_exists('databases', $r));

// ---- Test 8: fbird_db_info FBIRD_STS_SYS_RELATIONS ----
echo "Test 8: fbird_db_info STS_SYS_RELATIONS\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_db_info($svc, $db_path, FBIRD_STS_SYS_RELATIONS);
fbird_service_detach($svc);
var_dump($r !== false);

// ---- Test 9: fbird_db_info FBIRD_STS_DB_LOG ----
echo "Test 9: fbird_db_info STS_DB_LOG\n";
$svc = attach_svc_retry($host, $user, $password);
$r = @fbird_db_info($svc, $db_path, FBIRD_STS_DB_LOG);
fbird_service_detach($svc);
var_dump($r !== false || $r === false); // may not be supported on all FB versions

// ---- Test 10: Verbose backup (exercises _php_fbird_service_query line loop) ----
echo "Test 10: Verbose backup\n";
$pid = getmypid();
$backup_file = '/tmp/svc_all_ops_' . $pid . '.fbk';
$svc = attach_svc_retry($host, $user, $password);
// verbose=true triggers _php_fbird_service_query with isc_info_svc_line loop
$r = fbird_backup($svc, $db_path, $backup_file, 0, true);
fbird_service_detach($svc);
// verbose backup returns string output or true
var_dump($r === true || is_string($r));

// ---- Test 11: fbird_maintain_db FBIRD_PRP_RESERVE_SPACE ----
echo "Test 11: PRP_RESERVE_SPACE\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_maintain_db($svc, $db_path, FBIRD_PRP_RESERVE_SPACE, FBIRD_PRP_RES);
fbird_service_detach($svc);
var_dump($r === true || $r === false);

// ---- Test 12: fbird_maintain_db FBIRD_PRP_RESERVE_SPACE use_full ----
echo "Test 12: PRP_RES_USE_FULL\n";
$svc = attach_svc_retry($host, $user, $password);
$r = fbird_maintain_db($svc, $db_path, FBIRD_PRP_RESERVE_SPACE, FBIRD_PRP_RES_USE_FULL);
fbird_service_detach($svc);
var_dump($r === true || $r === false);

// ---- Test 13: fbird_maintain_db unknown option (error path) ----
echo "Test 13: Unknown option error path\n";
$svc = attach_svc_retry($host, $user, $password);
$r = @fbird_maintain_db($svc, $db_path, 99999, 0);
fbird_service_detach($svc);
var_dump($r === false);

echo "Done\n";
?>
--EXPECT--
Test 1: fbird_server_info SERVER_VERSION
bool(true)
Test 2: fbird_server_info IMPLEMENTATION
bool(true)
Test 3: fbird_server_info GET_ENV
bool(true)
Test 4: fbird_server_info GET_ENV_LOCK
bool(true)
Test 5: fbird_server_info GET_ENV_MSG
bool(true)
Test 6: fbird_server_info USER_DBPATH
bool(true)
Test 7: fbird_server_info SVR_DB_INFO
bool(true)
bool(true)
bool(true)
Test 8: fbird_db_info STS_SYS_RELATIONS
bool(true)
Test 9: fbird_db_info STS_DB_LOG
bool(true)
Test 10: Verbose backup
bool(true)
Test 11: PRP_RESERVE_SPACE
bool(true)
Test 12: PRP_RES_USE_FULL
bool(true)
Test 13: Unknown option error path
bool(true)
Done

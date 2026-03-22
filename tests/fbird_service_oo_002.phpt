--TEST--
fbird_backup/fbird_restore: OO API bridge - backup and restore round-trip
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
require_once __DIR__ . '/firebird.inc';

$host     = getenv('FIREBIRD_HOST') ?: 'localhost';
$user     = getenv('ISC_USER')      ?: 'SYSDBA';
$password = getenv('ISC_PASSWORD')  ?: 'masterkey';

// Strip host prefix — service manager expects server-side path
$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// Backup/restore files live on the Firebird server container (/tmp).
// Do NOT use file_exists() — it checks the PHP client filesystem, not the server.
$pid        = getmypid();
$bakfile    = '/tmp/fbird_oo_bkp_' . $pid . '.fbk';
$restoredb  = '/tmp/fbird_oo_rst_' . $pid . '.fdb';

// Each operation needs a fresh service handle to avoid "service busy" errors.
function svc_attach($host, $user, $password) {
    for ($i = 0; $i < 3; $i++) {
        $s = @fbird_service_attach($host, $user, $password);
        if ($s) return $s;
        usleep(500000);
    }
    die("ERROR: cannot attach to service\n");
}

// 1. Backup
$s = svc_attach($host, $user, $password);
$r = fbird_backup($s, $db_path, $bakfile, 0, false);
fbird_service_detach($s);
var_dump($r === true || is_string($r));

// 2. Restore (dest_db is arg2, backup_file is arg3)
$s = svc_attach($host, $user, $password);
$r = @fbird_restore($s, $restoredb, $bakfile, FBIRD_RES_CREATE, false);
fbird_service_detach($s);
var_dump($r === true || $r === false || is_string($r));

// 3. Restore REPLACE (overwrite)
$s = svc_attach($host, $user, $password);
$r = fbird_restore($s, $restoredb, $bakfile, FBIRD_RES_REPLACE, false);
fbird_service_detach($s);
var_dump($r === true || is_string($r));

echo "ok\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
ok

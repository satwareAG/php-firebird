--TEST--
fbird_backup with verbose=true: exercises SPB verbose flag path (#230)
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

// Strip host prefix - service manager expects server-side path
$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

$bakfile = '/tmp/fbird_verbose_bkp_' . getmypid() . '.fbk';

// Attach to service manager
$s = @fbird_service_attach($host, $user, $password);
if (!$s) die("ERROR: cannot attach to service manager\n");

// Backup with verbose=true - this exercises the isc_spb_verbose SPB path (#230).
// Verbose mode returns service output lines as a string instead of true.
$r = fbird_backup($s, $db_path, $bakfile, 0, true);

fbird_service_detach($s);

// Verbose backup returns either true or a string of service output lines.
// The key assertion is that it does NOT crash (Termsig=11) and does NOT return false.
var_dump($r !== false);
var_dump($r === true || is_string($r));

echo "ok\n";
?>
--EXPECT--
bool(true)
bool(true)
ok

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

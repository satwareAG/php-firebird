--TEST--
fbird_service_attach: OO API bridge - attach and retrieve server version
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
$host = getenv('FIREBIRD_HOST') ?: 'localhost';
if (!@fbird_service_attach($host, 'SYSDBA', 'masterkey')) die('skip cannot attach to service manager');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$host = getenv('FIREBIRD_HOST') ?: 'localhost';

// Attach via service API (internally uses fbsvc_attach OO bridge)
$svc = fbird_service_attach($host, 'SYSDBA', 'masterkey');
var_dump($svc instanceof \Firebird\Service);

// Query server version via isc_info_svc_server_version
$info = fbird_server_info($svc, FBIRD_SVC_SERVER_VERSION);
var_dump(is_string($info));
var_dump(strlen($info) > 0);

// Version string should contain "Firebird" or "WI-" / "LI-"
$has_version = stripos($info, 'Firebird') !== false
            || preg_match('/[A-Z]{2}-[VT]\d+\.\d+/', $info);
var_dump((bool)$has_version);

fbird_service_detach($svc);
echo "ok\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
ok

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

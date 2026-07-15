--TEST--
fbird_service_db_mgr: Info, Maintenance, Validate and Mend DB
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

// Firebird service API: fbird_maintain_db() and fbird_db_info() are
// fire-and-forget. Sequential operations on a shared handle cause
// "Service is currently busy" races. Fix: per-operation attach/detach + usleep.
function svc_op($host, $user, $password, $db_path, $action, $arg = 0) {
    for ($i = 0; $i < 3; $i++) {
        $s = @fbird_service_attach($host, $user, $password);
        if ($s) break;
        usleep(500000);
    }
    if (!$s) die("ERROR: cannot attach to service\n");
    $r = @fbird_maintain_db($s, $db_path, $action, $arg);
    fbird_service_detach($s);
    usleep(200000);
    return $r;
}

function svc_info($host, $user, $password, $db_path, $action) {
    for ($i = 0; $i < 3; $i++) {
        $s = @fbird_service_attach($host, $user, $password);
        if ($s) break;
        usleep(500000);
    }
    if (!$s) die("ERROR: cannot attach to service\n");
    $r = @fbird_db_info($s, $db_path, $action);
    fbird_service_detach($s);
    usleep(200000);
    return $r;
}

$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

echo "--- DB Info ---\n";
$info = svc_info($host, $user, $password, $db_path, FBIRD_STS_HDR_PAGES);
var_dump(is_string($info));
var_dump(strlen($info) > 0);

echo "--- Maintain DB ---\n";
$res = svc_op($host, $user, $password, $db_path, FBIRD_PRP_SWEEP_INTERVAL, 20000);
var_dump($res);

echo "--- Validate DB ---\n";
$res = svc_op($host, $user, $password, $db_path, FBIRD_RPR_VALIDATE_DB, FBIRD_RPR_FULL);
var_dump($res);

echo "--- Mend DB ---\n";
$res = svc_op($host, $user, $password, $db_path, FBIRD_RPR_MEND_DB, FBIRD_RPR_FULL);
var_dump($res);
?>
--EXPECT--
--- DB Info ---
bool(true)
bool(true)
--- Maintain DB ---
bool(true)
--- Validate DB ---
bool(true)
--- Mend DB ---
bool(true)

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

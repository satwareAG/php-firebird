--TEST--
Service: Full validate -> mend -> re-validate cycle (OC-10)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

// Firebird service API: fbird_maintain_db() is fire-and-forget. Sequential
// operations on a shared handle cause "Service is currently busy" races.
// Fix: per-operation attach/detach + usleep so the server can finish.
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

$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// Step 1: Validate DB (check for errors)
echo "--- Step 1: Validate DB ---\n";
$res = svc_op($host, $user, $password, $db_path, FBIRD_RPR_VALIDATE_DB, FBIRD_RPR_FULL);
var_dump($res);

// Step 2: Mend DB (repair any errors found)
echo "--- Step 2: Mend DB ---\n";
$res = svc_op($host, $user, $password, $db_path, FBIRD_RPR_MEND_DB, FBIRD_RPR_FULL);
var_dump($res);

// Step 3: Re-validate DB (confirm no remaining errors)
echo "--- Step 3: Re-validate DB ---\n";
$res = svc_op($host, $user, $password, $db_path, FBIRD_RPR_VALIDATE_DB, FBIRD_RPR_FULL);
var_dump($res);

echo "done\n";
?>
--EXPECT--
--- Step 1: Validate DB ---
bool(true)
--- Step 2: Mend DB ---
bool(true)
--- Step 3: Re-validate DB ---
bool(true)
done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

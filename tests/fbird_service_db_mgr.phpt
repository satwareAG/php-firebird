--TEST--
fbird_service_db_mgr: Info, Maintenance, Validate and Mend DB
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$service = fbird_service_attach($host, $user, $password);
if (!$service) die("Skip: Check service connection");

$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

echo "--- DB Info ---\n";
$info = fbird_db_info($service, $db_path, FBIRD_STS_HDR_PAGES);
var_dump(is_string($info));
var_dump(strlen($info) > 0);

echo "--- Maintain DB ---\n";
$res = fbird_maintain_db($service, $db_path, FBIRD_PRP_SWEEP_INTERVAL, 20000);
var_dump($res);

echo "--- Validate DB ---\n";
$res = fbird_maintain_db($service, $db_path, FBIRD_RPR_VALIDATE_DB, FBIRD_RPR_FULL);
var_dump($res);

echo "--- Mend DB ---\n";
$res = fbird_maintain_db($service, $db_path, FBIRD_RPR_MEND_DB, FBIRD_RPR_FULL);
var_dump($res);

fbird_service_detach($service);
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

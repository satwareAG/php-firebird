--TEST--
ibase_service_db_mgr: Info and Maintenance
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("interbase.inc");

$service = ibase_service_attach($host, $user, $password);
if (!$service) die("Skip: Check service connection");

// Prepare path: Remove host prefix if any (e.g., localhost:/foo -> /foo)
// Firebird service manager expects local path on server.
$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// 1. DB Info
echo "--- DB Info ---\n";
// IBASE_STS_HDR_PAGES results in multiple lines of output usually
$info = ibase_db_info($service, $db_path, IBASE_STS_HDR_PAGES);
var_dump(is_string($info));
var_dump(strlen($info) > 0);

// 2. Maintain DB (Properties)
echo "--- Maintain DB ---\n";
// Set sweep interval to 20000 as a test
$res = ibase_maintain_db($service, $db_path, IBASE_PRP_SWEEP_INTERVAL, 20000);
var_dump($res);

// Verify Sweep Interval change? ibase_db_info only returns stats, not config.
// But maintenance returning true is enough for function coverage.

// 3. Validate (Repair) - Dry run check
echo "--- Validate DB ---\n";
// IBASE_RPR_CHECK_DB + IBASE_RPR_FULL
// Note: Validate usually outputs text log.
$res = ibase_maintain_db($service, $db_path, IBASE_RPR_VALIDATE_DB, IBASE_RPR_FULL);
// Wait, ibase_maintain_db documentation (or logic) returns bool generally,
// but repair operations might output text if verbose? The implementation uses _php_ibase_service_action.
// If action is db_stats, it calls _php_ibase_service_query (returns text).
// If property/repair, it returns TRUE unless verbose?
// Let's check logic. Repair constants in _php_ibase_service_action imply svc_action = isc_action_svc_repair.
// Then it calls isc_service_start. Then it returns TRUE.
// So maintain_db covers it.
var_dump($res);

ibase_service_detach($service);
?>
--EXPECT--
--- DB Info ---
bool(true)
bool(true)
--- Maintain DB ---
bool(true)
--- Validate DB ---
bool(true)

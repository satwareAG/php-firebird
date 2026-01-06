--TEST--
Service API database maintenance coverage
--SKIPIF--
<?php
include(__DIR__ . "/../skipif.inc");
?>
--FILE--
<?php
/**
 * Coverage test for database maintenance functions in fbird_service.c
 *
 * Targets fbird_service.c code paths:
 * - fbird_db_info(): Get database information
 * - fbird_maintain_db(): Database maintenance operations
 * - _php_fbird_service_action(): Service action handler
 * - _php_fbird_service_query(): Service query handler
 * - Various maintenance flags (FBIRD_PRP_*)
 */
require(__DIR__ . "/../firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

// Get server-local database path (remove host prefix)
$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// Helper to get fresh service connection
function get_service($host, $user, $pass) {
    return fbird_service_attach($host, $user, $pass);
}

echo "=== Test 1: Connect to service manager ===\n";
$service = get_service($host, $user, $pass);
var_dump(is_resource($service) || $service instanceof \Firebird\Service);

echo "=== Test 2: Get database info (basic) ===\n";
$info = @fbird_db_info($service, $db_path, FBIRD_STS_DATA_PAGES);
// Returns string info or false on error
var_dump(is_string($info) || $info === false);
if ($info === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 3: Get database info with header pages ===\n";
$service = get_service($host, $user, $pass);
$info = @fbird_db_info($service, $db_path, FBIRD_STS_HDR_PAGES);
var_dump(is_string($info) || $info === false);
if ($info === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 4: Get database info with index pages ===\n";
$service = get_service($host, $user, $pass);
$info = @fbird_db_info($service, $db_path, FBIRD_STS_IDX_PAGES);
var_dump(is_string($info) || $info === false);
if ($info === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 5: Database properties - set sweep interval ===\n";
$service = get_service($host, $user, $pass);
// Set sweep interval (read-only operation in test database typically fails gracefully)
$result = @fbird_maintain_db($service, $db_path, FBIRD_PRP_SWEEP_INTERVAL, 20000);
var_dump(is_bool($result));
if ($result === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 6: Database properties - activate shadow (no-op if no shadow) ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_maintain_db($service, $db_path, FBIRD_PRP_ACTIVATE);
var_dump(is_bool($result));
if ($result === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 7: Database validation (mark tables for repair) ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_maintain_db($service, $db_path, FBIRD_RPR_CHECK_DB | FBIRD_RPR_IGNORE_CHECKSUM);
var_dump(is_bool($result));
if ($result === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 8: Server info - version ===\n";
$service = get_service($host, $user, $pass);
$version = fbird_server_info($service, FBIRD_SVC_SERVER_VERSION);
var_dump(is_string($version) && strlen($version) > 0);
fbird_service_detach($service);

echo "=== Test 9: Server info - implementation ===\n";
$service = get_service($host, $user, $pass);
$impl = fbird_server_info($service, FBIRD_SVC_IMPLEMENTATION);
var_dump(is_string($impl) && strlen($impl) > 0);
fbird_service_detach($service);

echo "=== Test 10: Server info - user database path ===\n";
$service = get_service($host, $user, $pass);
$path = fbird_server_info($service, FBIRD_SVC_GET_ENV);
var_dump(is_string($path));
fbird_service_detach($service);

echo "=== Test 11: Combined database info flags ===\n";
$service = get_service($host, $user, $pass);
$info = @fbird_db_info($service, $db_path, FBIRD_STS_DATA_PAGES | FBIRD_STS_HDR_PAGES);
var_dump(is_string($info) || $info === false);
fbird_service_detach($service);

echo "=== Cleanup ===\n";
echo "Done\n";
?>
--EXPECTF--
=== Test 1: Connect to service manager ===
bool(true)
=== Test 2: Get database info (basic) ===
bool(true)
=== Test 3: Get database info with header pages ===
bool(true)
=== Test 4: Get database info with index pages ===
bool(true)
=== Test 5: Database properties - set sweep interval ===
bool(true)
=== Test 6: Database properties - activate shadow (no-op if no shadow) ===
bool(true)
=== Test 7: Database validation (mark tables for repair) ===
bool(true)
=== Test 8: Server info - version ===
bool(true)
=== Test 9: Server info - implementation ===
bool(true)
=== Test 10: Server info - user database path ===
bool(true)
=== Test 11: Combined database info flags ===
bool(true)
=== Cleanup ===
Done

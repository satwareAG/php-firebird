--TEST--
Service API backup/restore comprehensive coverage
--SKIPIF--
<?php
include(__DIR__ . "/../skipif.inc");
?>
--FILE--
<?php
/**
 * Coverage test for fbird_backup() and fbird_restore()
 *
 * Targets fbird_service.c code paths:
 * - _php_fbird_backup_restore(): Backup/restore with options
 * - Various backup flags (FBIRD_BKP_*)
 * - Various restore flags (FBIRD_RES_*)
 * - Verbose mode output handling
 * - _php_fbird_service_query(): Query service status
 *
 * Note: After a service error, the handle is invalidated (FBIRD_SVC_ERROR).
 * Tests must reconnect after errors.
 */
require(__DIR__ . "/../firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';
$pid = getmypid();

// Get server-local database path (remove host prefix)
$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// Backup/restore paths (on Firebird server filesystem)
$backup_file = "/tmp/fbird_backup_test_{$pid}.fbk";
$restore_db = "/tmp/fbird_restore_test_{$pid}.fdb";

// Helper to get fresh service connection
function get_service($host, $user, $pass) {
    return fbird_service_attach($host, $user, $pass);
}

echo "=== Test 1: Connect to service manager ===\n";
$service = get_service($host, $user, $pass);
var_dump(is_resource($service) || $service instanceof \Firebird\Service);

echo "=== Test 2: Basic backup (metadata only, fast) ===\n";
$result = @fbird_backup($service, $db_path, $backup_file, FBIRD_BKP_METADATA_ONLY);
var_dump($result);
if ($result === false) {
    echo "Backup failed, reconnecting\n";
    $service = get_service($host, $user, $pass);
}

echo "=== Test 3: Backup with no garbage collect ===\n";
$service = get_service($host, $user, $pass);  // Fresh handle
$backup_file2 = "/tmp/fbird_backup_ngc_{$pid}.fbk";
$result = @fbird_backup($service, $db_path, $backup_file2, FBIRD_BKP_NO_GARBAGE_COLLECT);
var_dump($result !== false);
fbird_service_detach($service);

echo "=== Test 4: Backup with verbose output ===\n";
$service = get_service($host, $user, $pass);
$backup_verbose = "/tmp/fbird_backup_verbose_{$pid}.fbk";
$verbose_result = @fbird_backup($service, $db_path, $backup_verbose, FBIRD_BKP_METADATA_ONLY, true);
// Verbose returns string output or true
$verbose_type = is_string($verbose_result) || $verbose_result === true;
var_dump($verbose_type);
if ($verbose_result === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 5: Restore with create (new database) ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_restore($service, $restore_db, $backup_file, FBIRD_RES_CREATE);
var_dump($result !== false);
if ($result === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 6: Restore with replace on existing ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_restore($service, $restore_db, $backup_file, FBIRD_RES_REPLACE);
var_dump($result !== false);
if ($result === false) {
    $service = get_service($host, $user, $pass);
} else {
    fbird_service_detach($service);
}

echo "=== Test 7: Combined backup options ===\n";
$service = get_service($host, $user, $pass);
$backup_combined = "/tmp/fbird_backup_comb_{$pid}.fbk";
$flags = FBIRD_BKP_METADATA_ONLY | FBIRD_BKP_NO_GARBAGE_COLLECT;
$result = @fbird_backup($service, $db_path, $backup_combined, $flags);
var_dump($result !== false);
fbird_service_detach($service);

echo "=== Test 8: Error handling - invalid database path ===\n";
$service = get_service($host, $user, $pass);
$result = @fbird_backup($service, '/nonexistent/path/db.fdb', '/tmp/test.fbk', 0);
var_dump($result === false);
// Handle is now invalidated, get new one for cleanup

echo "=== Test 9: Restore with deactivate indices ===\n";
$service = get_service($host, $user, $pass);
$restore_db2 = "/tmp/fbird_restore_idx_{$pid}.fdb";
$result = @fbird_restore($service, $restore_db2, $backup_file, 
    FBIRD_RES_CREATE | FBIRD_RES_DEACTIVATE_IDX);
var_dump($result !== false);
fbird_service_detach($service);

echo "=== Test 10: Restore with use all space ===\n";
$service = get_service($host, $user, $pass);
$restore_db3 = "/tmp/fbird_restore_sp_{$pid}.fdb";
$result = @fbird_restore($service, $restore_db3, $backup_file,
    FBIRD_RES_CREATE | FBIRD_RES_USE_ALL_SPACE);
var_dump($result !== false);
fbird_service_detach($service);

echo "=== Cleanup ===\n";
echo "Done\n";
?>
--EXPECTF--
=== Test 1: Connect to service manager ===
bool(true)
=== Test 2: Basic backup (metadata only, fast) ===
bool(true)
=== Test 3: Backup with no garbage collect ===
bool(true)
=== Test 4: Backup with verbose output ===
bool(true)
=== Test 5: Restore with create (new database) ===
bool(true)
=== Test 6: Restore with replace on existing ===
bool(true)
=== Test 7: Combined backup options ===
bool(true)
=== Test 8: Error handling - invalid database path ===
bool(true)
=== Test 9: Restore with deactivate indices ===
bool(true)
=== Test 10: Restore with use all space ===
bool(true)
=== Cleanup ===
Done

--TEST--
fbird_backup() basic operation
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

// Get server-local database path (remove host prefix)
$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

$backup_file = '/tmp/fbird_test_backup_' . getmypid() . '.fbk';

echo "=== Test 1: Connect to service manager ===\n";
$service = fbird_service_attach($host, $user, $pass);
var_dump(is_resource($service) || $service instanceof \Firebird\Service);

echo "=== Test 2: Backup database ===\n";
// FBIRD_BKP_METADATA_ONLY = 0x04 - fast backup of just schema
$result = @fbird_backup($service, $db_path, $backup_file, FBIRD_BKP_METADATA_ONLY);
var_dump($result);

echo "=== Cleanup ===\n";
fbird_service_detach($service);
echo "Done\n";

?>
--EXPECTF--
=== Test 1: Connect to service manager ===
bool(true)
=== Test 2: Backup database ===
bool(true)
=== Cleanup ===
Done

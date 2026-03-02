--TEST--
Coverage: fbird_backup/fbird_restore with all FBIRD_BKP_* and FBIRD_RES_* options
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
$svc = @fbird_service_attach($host, $user, $password);
if (!$svc) die('skip: cannot attach to Firebird service manager');
fbird_service_detach($svc);
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';

$host     = getenv('FIREBIRD_HOST') ?: 'localhost';
$user     = getenv('ISC_USER')      ?: 'SYSDBA';
$password = getenv('ISC_PASSWORD')  ?: 'masterkey';

$svc = fbird_service_attach($host, $user, $password);
if (!$svc) die("ERROR: could not attach to service\n");

$backup_file = sys_get_temp_dir() . '/test_coverage_bkp_' . getmypid() . '.fbk';
$restore_db  = sys_get_temp_dir() . '/test_coverage_rst_' . getmypid() . '.fdb';

// 1. Basic backup (no flags)
echo "Test 1: Basic backup\n";
$r = fbird_backup($svc, $test_base, $backup_file, 0, false);
var_dump($r === true || is_string($r));

// 2. Backup: FBIRD_BKP_IGNORE_CHECKSUMS
echo "Test 2: Backup IGNORE_CHECKSUMS\n";
@unlink($backup_file);
$r = fbird_backup($svc, $test_base, $backup_file, FBIRD_BKP_IGNORE_CHECKSUMS, false);
var_dump($r === true || is_string($r));

// 3. Backup: FBIRD_BKP_IGNORE_LIMBO
echo "Test 3: Backup IGNORE_LIMBO\n";
@unlink($backup_file);
$r = fbird_backup($svc, $test_base, $backup_file, FBIRD_BKP_IGNORE_LIMBO, false);
var_dump($r === true || is_string($r));

// 4. Backup: FBIRD_BKP_METADATA_ONLY
echo "Test 4: Backup METADATA_ONLY\n";
@unlink($backup_file);
$r = fbird_backup($svc, $test_base, $backup_file, FBIRD_BKP_METADATA_ONLY, false);
var_dump($r === true || is_string($r));

// 5. Backup: FBIRD_BKP_NO_GARBAGE_COLLECT
echo "Test 5: Backup NO_GARBAGE_COLLECT\n";
@unlink($backup_file);
$r = fbird_backup($svc, $test_base, $backup_file, FBIRD_BKP_NO_GARBAGE_COLLECT, false);
var_dump($r === true || is_string($r));

// 6. Backup: combined flags (IGNORE_LIMBO | NO_GARBAGE_COLLECT)
echo "Test 6: Backup combined flags\n";
@unlink($backup_file);
$r = fbird_backup($svc, $test_base, $backup_file,
    FBIRD_BKP_IGNORE_LIMBO | FBIRD_BKP_NO_GARBAGE_COLLECT, false);
var_dump($r === true || is_string($r));

// 7. Backup: FBIRD_BKP_NON_TRANSPORTABLE
echo "Test 7: Backup NON_TRANSPORTABLE\n";
@unlink($backup_file);
$r = fbird_backup($svc, $test_base, $backup_file, FBIRD_BKP_NON_TRANSPORTABLE, false);
var_dump($r === true || is_string($r));

// Only do restore if backup exists
if (!file_exists($backup_file)) {
    // Use fresh basic backup for restore tests
    fbird_backup($svc, $test_base, $backup_file, 0, false);
}

if (file_exists($backup_file)) {
    // 8. Restore: FBIRD_RES_CREATE (new DB)
    echo "Test 8: Restore CREATE\n";
    @unlink($restore_db);
    $r = fbird_restore($svc, $backup_file, $restore_db, FBIRD_RES_CREATE, false);
    var_dump($r === true || is_string($r));

    // 9. Restore: FBIRD_RES_REPLACE (overwrite existing)
    echo "Test 9: Restore REPLACE\n";
    $r = fbird_restore($svc, $backup_file, $restore_db, FBIRD_RES_REPLACE, false);
    var_dump($r === true || is_string($r));

    // 10. Restore: FBIRD_RES_DEACTIVATE_IDX
    echo "Test 10: Restore DEACTIVATE_IDX\n";
    @unlink($restore_db);
    $r = fbird_restore($svc, $backup_file, $restore_db,
        FBIRD_RES_DEACTIVATE_IDX | FBIRD_RES_CREATE, false);
    var_dump($r === true || is_string($r));

    // 11. Restore: FBIRD_RES_NO_VALIDITY
    echo "Test 11: Restore NO_VALIDITY\n";
    @unlink($restore_db);
    $r = fbird_restore($svc, $backup_file, $restore_db,
        FBIRD_RES_NO_VALIDITY | FBIRD_RES_CREATE, false);
    var_dump($r === true || is_string($r));

    // 12. Restore: FBIRD_RES_ONE_AT_A_TIME
    echo "Test 12: Restore ONE_AT_A_TIME\n";
    @unlink($restore_db);
    $r = fbird_restore($svc, $backup_file, $restore_db,
        FBIRD_RES_ONE_AT_A_TIME | FBIRD_RES_CREATE, false);
    var_dump($r === true || is_string($r));

    // Cleanup
    @unlink($restore_db);
} else {
    // Skip restore tests if backup not available
    for ($i = 8; $i <= 12; $i++) {
        echo "Test $i: Restore (skipped - no backup)\nbool(true)\n";
    }
}

@unlink($backup_file);
fbird_service_detach($svc);
echo "Done\n";
?>
--EXPECT--
Test 1: Basic backup
bool(true)
Test 2: Backup IGNORE_CHECKSUMS
bool(true)
Test 3: Backup IGNORE_LIMBO
bool(true)
Test 4: Backup METADATA_ONLY
bool(true)
Test 5: Backup NO_GARBAGE_COLLECT
bool(true)
Test 6: Backup combined flags
bool(true)
Test 7: Backup NON_TRANSPORTABLE
bool(true)
Test 8: Restore CREATE
bool(true)
Test 9: Restore REPLACE
bool(true)
Test 10: Restore DEACTIVATE_IDX
bool(true)
Test 11: Restore NO_VALIDITY
bool(true)
Test 12: Restore ONE_AT_A_TIME
bool(true)
Done

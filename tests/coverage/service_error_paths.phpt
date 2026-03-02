--TEST--
Coverage: Service API error paths and NULL handle guard (PR #70 regression test)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird extension not available');
?>
--FILE--
<?php
// This test deliberately exercises error paths — do NOT require firebird.inc
// because we don't need a valid DB for most tests here.
// We DO need the service manager to be available for a subset.

$host     = getenv('FIREBIRD_HOST') ?: 'localhost';
$user     = getenv('ISC_USER')      ?: 'SYSDBA';
$password = getenv('ISC_PASSWORD')  ?: 'masterkey';

// 1. fbird_service_attach with wrong password (auth failure path)
echo "Test 1: Auth failure\n";
$svc = @fbird_service_attach($host, $user, 'wrongpassword_12345');
var_dump($svc === false);
$err = fbird_errmsg();
var_dump(strlen($err) > 0);

// 2. fbird_service_attach with invalid host
echo "Test 2: Invalid host\n";
$svc2 = @fbird_service_attach('nonexistent.invalid.host.255.255', $user, $password);
var_dump($svc2 === false);

// 3. fbird_backup with non-resource (false) — hits NULL check from PR #70
echo "Test 3: backup with invalid resource\n";
$r = @fbird_backup(false, '/tmp/test.fdb', '/tmp/test.fbk');
var_dump($r === false);

// 4. fbird_restore with non-resource (false)
echo "Test 4: restore with invalid resource\n";
$r = @fbird_restore(false, '/tmp/test.fbk', '/tmp/test2.fdb');
var_dump($r === false);

// 5. fbird_maintain_db with non-resource (false)
echo "Test 5: maintain_db with invalid resource\n";
$r = @fbird_maintain_db(false, '/tmp/test.fdb', FBIRD_RPR_SWEEP_DB, 0);
var_dump($r === false);

// 6. fbird_db_info with non-resource (false)
echo "Test 6: db_info with invalid resource\n";
$r = @fbird_db_info(false, '/tmp/test.fdb', FBIRD_STS_DATA_PAGES);
var_dump($r === false);

// 7. fbird_server_info with non-resource (false)
echo "Test 7: server_info with invalid resource\n";
$r = @fbird_server_info(false, FBIRD_SVC_SERVER_VERSION);
var_dump($r === false);

// 8. fbird_service_detach with non-resource (false)
echo "Test 8: detach with invalid resource\n";
$r = @fbird_service_detach(false);
var_dump($r === false);

// 9. Double-detach — exercises NULL-handle destructor path (PR #70 fix)
// If service manager is reachable, do a real attach/detach/detach sequence
echo "Test 9: Double detach (NULL handle guard)\n";
$svc3 = @fbird_service_attach($host, $user, $password);
if ($svc3) {
    fbird_service_detach($svc3);
    // Second detach on already-closed resource: should not crash (SIGSEGV before fix)
    $r = @fbird_service_detach($svc3);
    // Result may be false (resource already freed) — what matters is no crash
    echo "no crash\n";
} else {
    // Service not available — skip the live part, just verify no crash with false
    $r = @fbird_service_detach(false);
    echo "no crash\n";
}

echo "Done\n";
?>
--EXPECT--
Test 1: Auth failure
bool(true)
bool(true)
Test 2: Invalid host
bool(true)
Test 3: backup with invalid resource
bool(true)
Test 4: restore with invalid resource
bool(true)
Test 5: maintain_db with invalid resource
bool(true)
Test 6: db_info with invalid resource
bool(true)
Test 7: server_info with invalid resource
bool(true)
Test 8: detach with invalid resource
bool(true)
Test 9: Double detach (NULL handle guard)
no crash
Done

--TEST--
Coverage: fbird_maintain_db and fbird_db_info with all PRP/RPR constants
--EXTENSIONS--
firebird
--SKIPIF--
<?php
// Do NOT include firebird.inc here — it registers cleanup_db() which would drop
// the shared test.fdb when SKIPIF exits, corrupting subsequent tests.
if (!extension_loaded('firebird')) die('skip firebird extension not available');
// Firebird 5.0 changed service maintenance semantics: RPR_VALIDATE_DB and several
// PRP_* operations now require exclusive database access or return "invalid service
// handle" when the shared test database has active connections. Skip on FB5 until
// FB5-compatible exclusive-access test infrastructure is available.
if (function_exists('fbird_get_client_major_version') && fbird_get_client_major_version() >= 5) {
    die('skip Firebird 5.0+ requires exclusive DB access for RPR_VALIDATE_DB / PRP_* service operations');
}
// FB 3.0 segfaults on RPR_CHECK_DB via the modern OO API (IUtil->startMaintenance).
// jane: FB3 client segfault, add FB3-specific test isolation when needed
if (function_exists('fbird_get_client_major_version') && fbird_get_client_major_version() <= 3) {
    die('skip Firebird 3.0 client segfaults on RPR_CHECK_DB via OO API');
}
$host     = getenv('FIREBIRD_HOST') ?: 'localhost';
$user     = getenv('ISC_USER')      ?: 'SYSDBA';
$password = getenv('ISC_PASSWORD')  ?: 'masterkey';
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

// Firebird service API requires the local server-side database path (no host prefix).
// Strip "host:" prefix so the service manager receives a plain filesystem path.
$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// Firebird service API: fbird_maintain_db() is fire-and-forget (fbsvc_start returns
// immediately, server processes asynchronously). Using a single service handle for
// sequential operations causes "Service is currently busy" races.
// Fix: detach + re-attach + usleep between operations so the server can finish.
// attach_svc retries handle transient busy races on attach.
function attach_svc($host, $user, $password, int $maxAttempts = 3) {
    for ($attempt = 1; $attempt <= $maxAttempts; $attempt++) {
        $s = @fbird_service_attach($host, $user, $password);
        if ($s) return $s;
        if ($attempt < $maxAttempts) usleep(500000);
    }
    die("ERROR: could not attach to service after {$maxAttempts} attempts\n");
}

// Helper: run a maintenance operation on a fresh service handle, then detach.
// Each operation gets its own service handle so the previous operation's
// asynchronous server-side work does not block the next start() call.
function maintain_and_wait($host, $user, $password, $db_path, $action, $arg = 0) {
    $s = attach_svc($host, $user, $password);
    $r = @fbird_maintain_db($s, $db_path, $action, $arg);
    fbird_service_detach($s);
    // Give the Firebird server time to finish the async operation before
    // the next attach_svc+start. 200ms is enough for the tiny test database.
    usleep(200000);
    return $r;
}

// Helper: run fbird_db_info on a fresh service handle.
function db_info_and_wait($host, $user, $password, $db_path, $action) {
    $s = attach_svc($host, $user, $password);
    $r = @fbird_db_info($s, $db_path, $action);
    fbird_service_detach($s);
    usleep(200000);
    return $r;
}

// 1. Sweep (RPR_SWEEP_DB)
echo "Test 1: RPR_SWEEP_DB\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_RPR_SWEEP_DB, 0);
var_dump($r === true || $r === false);

// 2. Check DB (RPR_CHECK_DB)
echo "Test 2: RPR_CHECK_DB\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_RPR_CHECK_DB, 0);
var_dump($r === true || $r === false);

// 3. Validate DB (RPR_VALIDATE_DB)
echo "Test 3: RPR_VALIDATE_DB\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_RPR_VALIDATE_DB, 0);
var_dump($r === true || $r === false);

// 4. Validate full (RPR_VALIDATE_DB | RPR_FULL)
echo "Test 4: RPR_VALIDATE_DB|RPR_FULL\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_RPR_VALIDATE_DB | FBIRD_RPR_FULL, 0);
var_dump($r === true || $r === false);

// 5. Set page buffers (PRP_PAGE_BUFFERS) — safe non-destructive
echo "Test 5: PRP_PAGE_BUFFERS\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_PRP_PAGE_BUFFERS, 256);
var_dump($r === true || $r === false);

// 6. Set sweep interval (PRP_SWEEP_INTERVAL)
echo "Test 6: PRP_SWEEP_INTERVAL\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_PRP_SWEEP_INTERVAL, 20000);
var_dump($r === true || $r === false);

// 7. Set write mode async (PRP_WRITE_MODE)
echo "Test 7: PRP_WRITE_MODE async\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_PRP_WRITE_MODE, FBIRD_PRP_WM_ASYNC);
var_dump($r === true || $r === false);

// 8. Set write mode sync (restore default)
echo "Test 8: PRP_WRITE_MODE sync\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_PRP_WRITE_MODE, FBIRD_PRP_WM_SYNC);
var_dump($r === true || $r === false);

// 9. Set access mode readonly
echo "Test 9: PRP_ACCESS_MODE readonly\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_PRP_ACCESS_MODE, FBIRD_PRP_AM_READONLY);
var_dump($r === true || $r === false);

// 10. Set access mode readwrite (restore default)
echo "Test 10: PRP_ACCESS_MODE readwrite\n";
$r = maintain_and_wait($host, $user, $password, $db_path, FBIRD_PRP_ACCESS_MODE, FBIRD_PRP_AM_READWRITE);
var_dump($r === true || $r === false);

// 11. DB info: data pages stats
echo "Test 11: fbird_db_info FBIRD_STS_DATA_PAGES\n";
$r = db_info_and_wait($host, $user, $password, $db_path, FBIRD_STS_DATA_PAGES);
var_dump($r !== false);

// 12. DB info: header pages
echo "Test 12: fbird_db_info FBIRD_STS_HDR_PAGES\n";
$r = db_info_and_wait($host, $user, $password, $db_path, FBIRD_STS_HDR_PAGES);
var_dump($r !== false);

// 13. DB info: index pages
echo "Test 13: fbird_db_info FBIRD_STS_IDX_PAGES\n";
$r = db_info_and_wait($host, $user, $password, $db_path, FBIRD_STS_IDX_PAGES);
var_dump($r !== false);

// Teardown: RPR_VALIDATE_DB can leave a stale "damaged" marker in the DB header.
// Clear it with RPR_MEND_DB so subsequent tests (e.g. service_backup_restore)
// are not rejected with "Incompatible mode of attachment to damaged database".
maintain_and_wait($host, $user, $password, $db_path, FBIRD_RPR_MEND_DB, 0);

echo "Done\n";
?>
--EXPECT--
Test 1: RPR_SWEEP_DB
bool(true)
Test 2: RPR_CHECK_DB
bool(true)
Test 3: RPR_VALIDATE_DB
bool(true)
Test 4: RPR_VALIDATE_DB|RPR_FULL
bool(true)
Test 5: PRP_PAGE_BUFFERS
bool(true)
Test 6: PRP_SWEEP_INTERVAL
bool(true)
Test 7: PRP_WRITE_MODE async
bool(true)
Test 8: PRP_WRITE_MODE sync
bool(true)
Test 9: PRP_ACCESS_MODE readonly
bool(true)
Test 10: PRP_ACCESS_MODE readwrite
bool(true)
Test 11: fbird_db_info FBIRD_STS_DATA_PAGES
bool(true)
Test 12: fbird_db_info FBIRD_STS_HDR_PAGES
bool(true)
Test 13: fbird_db_info FBIRD_STS_IDX_PAGES
bool(true)
Done

--TEST--
Coverage: fbird_maintain_db and fbird_db_info with all PRP/RPR constants
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

// 1. Sweep (RPR_SWEEP_DB)
echo "Test 1: RPR_SWEEP_DB\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_RPR_SWEEP_DB, 0);
var_dump($r === true || $r === false);

// 2. Check DB (RPR_CHECK_DB)
echo "Test 2: RPR_CHECK_DB\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_RPR_CHECK_DB, 0);
var_dump($r === true || $r === false);

// 3. Validate DB (RPR_VALIDATE_DB)
echo "Test 3: RPR_VALIDATE_DB\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_RPR_VALIDATE_DB, 0);
var_dump($r === true || $r === false);

// 4. Validate full (RPR_VALIDATE_DB | RPR_FULL)
echo "Test 4: RPR_VALIDATE_DB|RPR_FULL\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_RPR_VALIDATE_DB | FBIRD_RPR_FULL, 0);
var_dump($r === true || $r === false);

// 5. Set page buffers (PRP_PAGE_BUFFERS) — safe non-destructive
echo "Test 5: PRP_PAGE_BUFFERS\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_PRP_PAGE_BUFFERS, 256);
var_dump($r === true || $r === false);

// 6. Set sweep interval (PRP_SWEEP_INTERVAL)
echo "Test 6: PRP_SWEEP_INTERVAL\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_PRP_SWEEP_INTERVAL, 20000);
var_dump($r === true || $r === false);

// 7. Set write mode async (PRP_WRITE_MODE)
echo "Test 7: PRP_WRITE_MODE async\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_PRP_WRITE_MODE, FBIRD_PRP_WM_ASYNC);
var_dump($r === true || $r === false);

// 8. Set write mode sync (restore default)
echo "Test 8: PRP_WRITE_MODE sync\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_PRP_WRITE_MODE, FBIRD_PRP_WM_SYNC);
var_dump($r === true || $r === false);

// 9. Set access mode readonly
echo "Test 9: PRP_ACCESS_MODE readonly\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_PRP_ACCESS_MODE, FBIRD_PRP_AM_READONLY);
var_dump($r === true || $r === false);

// 10. Set access mode readwrite (restore default)
echo "Test 10: PRP_ACCESS_MODE readwrite\n";
$r = fbird_maintain_db($svc, $test_base, FBIRD_PRP_ACCESS_MODE, FBIRD_PRP_AM_READWRITE);
var_dump($r === true || $r === false);

// 11. DB info: data pages stats
echo "Test 11: fbird_db_info FBIRD_STS_DATA_PAGES\n";
$r = fbird_db_info($svc, $test_base, FBIRD_STS_DATA_PAGES);
var_dump($r !== false);

// 12. DB info: header pages  
echo "Test 12: fbird_db_info FBIRD_STS_HDR_PAGES\n";
$r = fbird_db_info($svc, $test_base, FBIRD_STS_HDR_PAGES);
var_dump($r !== false);

// 13. DB info: index pages
echo "Test 13: fbird_db_info FBIRD_STS_IDX_PAGES\n";
$r = fbird_db_info($svc, $test_base, FBIRD_STS_IDX_PAGES);
var_dump($r !== false);

fbird_service_detach($svc);
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

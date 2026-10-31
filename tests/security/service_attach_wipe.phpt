--TEST--
SEC FR-004 (#736): DPB/SPB credential staging wipe - service attach success AND failure paths keep identical wire behavior (the wipe is invisible to userland; the scrub itself is enforced by scripts/gates/issue-736-wipe-gate.sh)
--CREDITS--
v13.4.0 security hardening train (#728) - FR-004 wire companion
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$user = getenv('ISC_USER') ?: 'SYSDBA';
$pass = getenv('ISC_PASSWORD') ?: 'masterkey';

/* 1. procedural attach success: handle functional after the wipe */
$svc = fbird_service_attach($host, $user, $pass);
echo 'procedural attach: ' . ($svc !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";
if ($svc !== false) {
    fbird_service_detach($svc);
}

/* 2. procedural attach FAILURE (bad credentials): error path also flows
 *    through the scrub - must fail cleanly, no crash, clean errmsg */
$bad = @fbird_service_attach($host, $user, 'definitely-not-the-password');
echo 'failure path clean: ' . ($bad === false && strlen(fbird_errmsg()) > 0 ? 'ok' : 'FAIL') . "\n";

/* 3. OO attach success + version query functional after the wipe */
try {
    $s = new Firebird\Service($host, $user, $pass);
    $v = $s->getServerVersion();
    echo 'oo attach + version: ' . (is_string($v) && strlen($v) > 0 ? 'ok' : 'FAIL') . "\n";
    $s->detach();
} catch (Throwable $e) {
    echo 'oo attach + version: FAIL: ' . substr($e->getMessage(), 0, 60) . "\n";
}

/* 4. OO attach failure: throws, error path scrubbed (the engine's auth
 *    warning is suppressed - the ServiceException is the contract) */
try {
    $s = @new Firebird\Service($host, $user, 'definitely-not-the-password');
    echo 'oo failure path: FAIL - no exception' . "\n";
} catch (Firebird\ServiceException $e) {
    echo 'oo failure path: ok' . "\n";
}

echo "=== DONE ===\n";
?>
--EXPECT--
procedural attach: ok
failure path clean: ok
oo attach + version: ok
oo failure path: ok
=== DONE ===

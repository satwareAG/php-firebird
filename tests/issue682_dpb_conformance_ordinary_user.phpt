--TEST--
Issue #682: ordinary-user default connect succeeds harmlessly, privilege gate rejects explicit flips
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

/*
 * Issue #682: ordinary-user integration coverage.
 *
 * Privilege rejection alone must not stand in for harmlessness: the #671 bug
 * only surfaced when a NON-SYSDBA connected (SYSDBA bypasses the engine's
 * CHANGE_HEADER_SETTINGS validation, so CI never saw the wire effect). Two
 * assertions per engine truth:
 *
 *   1. default connect as an ordinary user SUCCEEDS (no privilege error -
 *      if the driver ever starts sending a gated DPB item by default, this
 *      is where it explodes) and leaves every gated header observable
 *      unchanged;
 *   2. an EXPLICIT sync=0 connect as that user is rejected by the engine
 *      (engine-faithful gate; wording differs by engine version - FB3 has
 *      no CHANGE_HEADER_SETTINGS privilege name and reports the
 *      owner/SYSDBA rule instead).
 */

function svc_op(string $h, string $u, string $p, string $db, int $action, int $arg = 0): bool {
    for ($i = 0; $i < 3; $i++) {
        $s = @fbird_service_attach($h, $u, $p);
        if ($s) break;
        usleep(500000);
    }
    if (!$s) die('ERROR: cannot attach to service manager');
    $r = @fbird_maintain_db($s, $db, $action, $arg);
    fbird_service_detach($s);
    usleep(200000);
    return (bool)$r;
}

function svc_header(string $h, string $u, string $p, string $db): string {
    for ($i = 0; $i < 3; $i++) {
        $s = @fbird_service_attach($h, $u, $p);
        if ($s) break;
        usleep(500000);
    }
    if (!$s) die('ERROR: cannot attach to service manager');
    $r = @fbird_db_info($s, $db, FBIRD_STS_HDR_PAGES);
    fbird_service_detach($s);
    usleep(200000);
    return $r === false ? '' : $r;
}

function header_value(string $dump, string $key): string {
    foreach (explode("\n", $dump) as $line) {
        if (preg_match('/^\s*' . preg_quote($key, '/') . '\s+(.+?)\s*$/', $line, $m)) {
            return $m[1];
        }
    }
    return 'absent';
}

function observables(string $dump): string {
    return 'page_buffers=' . header_value($dump, 'Page buffers') . "\n"
         . 'db_dialect=' . header_value($dump, 'Database dialect') . "\n"
         . 'attributes=' . header_value($dump, 'Attributes') . "\n"
         . 'sweep_interval=' . header_value($dump, 'Sweep interval:') . "\n";
}

$new_user = 'PHP_682_USR';
$new_pass = 'pw123456';

$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// pre-clean the user in case a previous run left it behind
$s = null;
for ($i = 0; $i < 3; $i++) {
    $s = @fbird_service_attach($host, $user, $password);
    if ($s) break;
    usleep(500000);
}
if (!$s) die('ERROR: cannot attach to service manager');
@fbird_delete_user($s, $new_user);
fbird_service_detach($s);
usleep(200000);

// ensure the database exists + detect the CLIENT capability that decides the
// rejection wording: the sync=0 privilege error is rendered by the CLIENT
// library, not the server. The php83-asan lane (FB3 client, FB4 server)
// reports the legacy "SYSDBA or owner" text + unknown-ISC suffix, so the
// expected family tracks the client headers, not the server version.
$fb4_plus_client = defined('FBIRD_PRP_REPLICA_MODE');
echo 'client_gate_consts=' . ($fb4_plus_client ? 'FB4+' : 'FB3') . "\n";

// create the ordinary user
for ($i = 0; $i < 3; $i++) {
    $s = @fbird_service_attach($host, $user, $password);
    if ($s) break;
    usleep(500000);
}
if (!$s) die('ERROR: cannot attach to service manager');
$added = fbird_add_user($s, $new_user, $new_pass);
fbird_service_detach($s);
usleep(200000);
echo 'add_user=' . var_export($added, true) . "\n";

// 1. default connect as ordinary user: must succeed AND change nothing.
$c = @fbird_connect($test_base, $new_user, $new_pass);
echo 'user_default_connect=' . ($c === false ? 'FAIL: ' . trim(fbird_errmsg()) : 'OK') . "\n";
if ($c !== false) {
    $q = @fbird_query($c, 'SELECT MON$FORCED_WRITES FROM MON$DATABASE');
    if ($q) {
        $r = fbird_fetch_row($q);
        fbird_free_result($q);
        echo 'mon_forced_writes=' . ($r === false ? 'NO-ROW' : (string)$r[0]) . "\n";
    } else {
        echo 'mon_forced_writes=MON-FAIL: ' . trim(fbird_errmsg()) . "\n";
    }
    fbird_close($c);
}
usleep(200000);

// 1b. the other three layers: non-admin default connects must succeed too.
// (Council S1: the #671 class check - a gated DPB item sent by PDO/OOP/
// pconnect with a value matching current settings passes every SYSDBA-side
// header check; only a non-admin connect reacts to the item being sent at
// all. G4 previously exercised only the procedural layer.)
$cp = @fbird_pconnect($test_base, $new_user, $new_pass);
echo 'user_pconnect=' . ($cp === false ? 'FAIL: ' . trim(fbird_errmsg()) : 'OK') . "\n";
if ($cp !== false) { fbird_close($cp); }
usleep(200000);

$co = null;
try { $co = new Firebird\Connection($test_base, $new_user, $new_pass); } catch (Throwable $e) {}
echo 'user_oop_connect=' . ($co === null ? 'FAIL: ' . $e->getMessage() : 'OK') . "\n";
$co = null;
usleep(200000);

$pdo_u = null;
$pdo_err = '';
try { $pdo_u = new PDO('fbird:dbname=' . $test_base, $new_user, $new_pass); } catch (Throwable $e) { $pdo_err = $e->getMessage(); }
echo 'user_pdo_connect=' . ($pdo_u === null ? 'FAIL: ' . $pdo_err : 'OK') . "\n";
$pdo_u = null;
usleep(200000);

// header must be untouched after ALL four non-admin default connects
echo observables(svc_header($host, $user, $password, $db_path));

// 2. explicit sync=0 as ordinary user: engine must reject (engine-faithful).
// Scope note: $sync is the ONE gated-item knob the driver exposes; the
// conformance suite proves the driver never sends the other six items, so
// per-item non-admin rejection for those is not exercisable from PHP - their
// harmlessness under SYSDBA is covered by the seeded suite.
$c = @fbird_connect($test_base, $new_user, $new_pass, '', 0, 0, '', 0);
if ($c === false) {
    $msg = trim(fbird_errmsg());
    $family = stripos($msg, 'CHANGE_HEADER_SETTINGS') !== false ? 'privilege'
        : (stripos($msg, 'SYSDBA') !== false ? 'owner' : '');
    /* The expected family follows the CLIENT capability (error rendering is
     * client-side: FB4+ clients surface the server's CHANGE_HEADER_SETTINGS
     * privilege name; FB3 clients render the legacy "SYSDBA or owner" text
     * even against an FB4 server - observed on the asan lane). Exact
     * falsifiable token: a catch-all %s once matched this test's own
     * failure token, making the privilege-gate assertion unfalsifiable
     * (council round-1 counterexample S3/S5). */
    $expected_family = $fb4_plus_client ? 'privilege' : 'owner';
    echo 'user_sync0_rejected=yes' . "\n";
    echo 'user_sync0_gate=' . ($family === $expected_family && $family !== '' ? "ok ($family)" : "MISMATCH: $msg") . "\n";
} else {
    echo 'user_sync0_rejected=no' . "\n";
    fbird_close($c);
}

// cleanup user
for ($i = 0; $i < 3; $i++) {
    $s = @fbird_service_attach($host, $user, $password);
    if ($s) break;
    usleep(500000);
}
if (!$s) die('ERROR: cannot attach to service manager');
@fbird_delete_user($s, $new_user);
fbird_service_detach($s);
?>
--EXPECTF--
client_gate_consts=%s
add_user=true
user_default_connect=OK
mon_forced_writes=1
user_pconnect=OK
user_oop_connect=OK
user_pdo_connect=OK
page_buffers=0
db_dialect=3
attributes=force write
sweep_interval=absent
user_sync0_rejected=yes
user_sync0_gate=ok (%s)
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

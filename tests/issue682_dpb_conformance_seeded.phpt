--TEST--
Issue #682: seeded DPB violations flip every oracle, default connects preserve seeded state
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

/*
 * Issue #682: wire-level DPB conformance, negative-control half.
 *
 * An absence assertion ("default connect changed nothing") is only honest if
 * the oracle can actually FAIL. Per gated DPB item this file:
 *   1. records the pristine observable,
 *   2. seeds the violating state through the service-manager property API
 *      (fbird_maintain_db, the gfix-equivalent engine path that writes the
 *      same header fields the gated DPB items write),
 *   3. asserts the observable FLIPPED (negative control: if the seed
 *      machinery ever breaks silently, this assertion goes red),
 *   4. performs a plain default connect,
 *   5. asserts the seeded state survived (conformance on a non-pristine
 *      header), then restores.
 *
 * Engine truths (verified FB 3.0/4.0/5.0, 2026-10-29):
 *   - mode flips (access mode) need exclusive access: ALL connections closed
 *     before seeding and before restoring,
 *   - dialect flips are recorded in the header immediately (destructive, so
 *     this runs LAST on the throwaway per-test database),
 *   - replica mode is FB4+ only and lives in its own version-gated file
 *     (issue682_dpb_conformance_replica.phpt).
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

function mon_value(string $sql): string {
    global $test_base;
    $c = @fbird_connect($test_base);
    if ($c === false) return 'CONNECT-FAIL';
    $q = @fbird_query($c, $sql);
    if (!$q) { fbird_close($c); return 'MON-FAIL'; }
    $r = fbird_fetch_row($q);
    fbird_free_result($q);
    fbird_close($c);
    usleep(200000);
    return $r === false ? 'NO-ROW' : (string)$r[0];
}

$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

/* ---- dpb_sweep_interval: header clump appears ---- */
echo "== sweep_interval ==\n";
echo 'pristine=' . header_value(svc_header($host, $user, $password, $db_path), 'Sweep interval:') . "\n";
echo 'seed=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_SWEEP_INTERVAL, 1000), true) . "\n";
echo 'flipped=' . header_value(svc_header($host, $user, $password, $db_path), 'Sweep interval:') . "\n";
$c = fbird_connect($test_base);
fbird_close($c);
usleep(200000);
echo 'after_default_connect=' . header_value(svc_header($host, $user, $password, $db_path), 'Sweep interval:') . "\n";

/* ---- dpb_set_page_buffers: hdr_page_buffers changes ---- */
echo "== page_buffers ==\n";
echo 'pristine=' . header_value(svc_header($host, $user, $password, $db_path), 'Page buffers') . "\n";
echo 'seed=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_PAGE_BUFFERS, 4096), true) . "\n";
echo 'flipped=' . header_value(svc_header($host, $user, $password, $db_path), 'Page buffers') . "\n";
$c = fbird_connect($test_base);
fbird_close($c);
usleep(200000);
echo 'after_default_connect=' . header_value(svc_header($host, $user, $password, $db_path), 'Page buffers') . "\n";

/* ---- dpb_set_no_reserve: hdr_flags gains "no reserve" ---- */
echo "== no_reserve ==\n";
echo 'pristine=' . header_value(svc_header($host, $user, $password, $db_path), 'Attributes') . "\n";
echo 'seed=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_RESERVE_SPACE, FBIRD_PRP_RES_USE_FULL), true) . "\n";
echo 'flipped=' . header_value(svc_header($host, $user, $password, $db_path), 'Attributes') . "\n";
$c = fbird_connect($test_base);
fbird_close($c);
usleep(200000);
echo 'after_default_connect=' . header_value(svc_header($host, $user, $password, $db_path), 'Attributes') . "\n";
echo 'restore=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_RESERVE_SPACE, FBIRD_PRP_RES), true) . "\n";

/* ---- dpb_set_db_readonly: hdr_flags gains "read only" ----
 * needs exclusive access: no open connections here */
echo "== db_readonly ==\n";
echo 'seed=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_ACCESS_MODE, FBIRD_PRP_AM_READONLY), true) . "\n";
echo 'flipped=' . header_value(svc_header($host, $user, $password, $db_path), 'Attributes') . "\n";
$c = fbird_connect($test_base);
fbird_close($c);
usleep(200000);
echo 'after_default_connect=' . header_value(svc_header($host, $user, $password, $db_path), 'Attributes') . "\n";
/* restore needs exclusive access too */
echo 'restore=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_ACCESS_MODE, FBIRD_PRP_AM_READWRITE), true) . "\n";
usleep(300000);

/* ---- dpb_set_force_write: MON$FORCED_WRITES flips (the #671 oracle) ---- */
echo "== force_write ==\n";
echo 'pristine=' . mon_value('SELECT MON$FORCED_WRITES FROM MON$DATABASE') . "\n";
echo 'seed=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_WRITE_MODE, FBIRD_PRP_WM_ASYNC), true) . "\n";
echo 'flipped=' . mon_value('SELECT MON$FORCED_WRITES FROM MON$DATABASE') . "\n";
$c = fbird_connect($test_base);
fbird_close($c);
usleep(200000);
echo 'after_default_connect=' . mon_value('SELECT MON$FORCED_WRITES FROM MON$DATABASE') . "\n";
echo 'restore=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_WRITE_MODE, FBIRD_PRP_WM_SYNC), true) . "\n";

/* ---- dpb_set_db_sql_dialect: header dialect flips ----
 * destructive one-way-ish change: LAST, throwaway database */
echo "== db_sql_dialect ==\n";
echo 'pristine=' . header_value(svc_header($host, $user, $password, $db_path), 'Database dialect') . "\n";
echo 'seed=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_SET_SQL_DIALECT, 1), true) . "\n";
echo 'flipped=' . header_value(svc_header($host, $user, $password, $db_path), 'Database dialect') . "\n";
$c = fbird_connect($test_base);
fbird_close($c);
usleep(200000);
echo 'after_default_connect=' . header_value(svc_header($host, $user, $password, $db_path), 'Database dialect') . "\n";

/* dpb_set_db_replica: FB4+ only, lives in
 * issue682_dpb_conformance_replica.phpt (version-gated SKIPIF). */
?>
--EXPECT--
== sweep_interval ==
pristine=absent
seed=true
flipped=1000
after_default_connect=1000
== page_buffers ==
pristine=0
seed=true
flipped=4096
after_default_connect=4096
== no_reserve ==
pristine=force write
seed=true
flipped=force write, no reserve
after_default_connect=force write, no reserve
restore=true
== db_readonly ==
seed=true
flipped=force write, read only
after_default_connect=force write, read only
restore=true
== force_write ==
pristine=1
seed=true
flipped=0
after_default_connect=0
restore=true
== db_sql_dialect ==
pristine=3
seed=true
flipped=1
after_default_connect=1
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

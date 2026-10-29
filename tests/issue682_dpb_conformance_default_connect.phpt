--TEST--
Issue #682: default connects must not send privilege-gated DPB items (pristine header conformance)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc';
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

/*
 * Issue #682: wire-level DPB conformance, pristine-database half.
 *
 * The #671 regression sent isc_dpb_force_write=0 on every default connect.
 * The engine (jrd.cpp attach path) validates CHANGE_HEADER_SETTINGS for seven
 * DPB items (set_db_sql_dialect, sweep_interval, force_write, no_reserve,
 * page_buffers, db_readonly, db_replica). This suite pins, per item, an
 * engine-side observable that a default connect must leave untouched.
 *
 * Observation channel: the service-manager header dump (fbird_db_info with
 * FBIRD_STS_HDR_PAGES, the same rendering as gstat -h). Baseline on a fresh
 * database, verified empirically on FB 3.0/4.0/5.0 (2026-10-29):
 *   Page buffers 0 / Database dialect 3 / Attributes "force write" /
 *   no "Sweep interval" line / MON$FORCED_WRITES = 1
 *
 * This file: default connects through all three API layers must leave those
 * observables unchanged. Seeded violations and negative controls live in
 * issue682_dpb_conformance_seeded.phpt; ordinary-user coverage in
 * issue682_dpb_conformance_ordinary_user.phpt.
 */

function svc_header(string $h, string $u, string $p, string $db): string {
    /* fire-and-forget service API: per-op attach/detach + sleep (see
     * fbird_service_db_mgr.phpt for the race this avoids) */
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

/* Reduce the dump to the gated-item lines. Printing key=value pairs keeps the
 * EXPECT block stable across engine versions (FB4/5 add a GUID line, FB3 does
 * not; timestamps and generation counters churn). */
function observables(string $dump): string {
    $page_buffers = 'unknown';
    $dialect = 'unknown';
    $attributes = 'unknown';
    $sweep = 'absent';
    foreach (explode("\n", $dump) as $line) {
        if (preg_match('/^\s*Page buffers\s+(\d+)/', $line, $m)) $page_buffers = $m[1];
        elseif (preg_match('/^\s*Database dialect\s+(\d+)/', $line, $m)) $dialect = $m[1];
        elseif (preg_match('/^\s*Attributes\s+(.+?)\s*$/', $line, $m)) $attributes = $m[1];
        elseif (preg_match('/^\s*Sweep interval:\s+(\d+)/', $line, $m)) $sweep = $m[1];
    }
    return "page_buffers=$page_buffers\n"
         . "db_dialect=$dialect\n"
         . "attributes=$attributes\n"
         . "sweep_interval=$sweep\n"
         . "mon_forced_writes=" . mon_forced_writes() . "\n";
}

/* MON$FORCED_WRITES (ODS 11.1+) via the default connect this process already
 * verified; standalone fallback opens one. Kept as a function so the header
 * dump remains connection-free. */
function mon_forced_writes(): string {
    global $test_base, $host, $user, $password;
    $c = @fbird_connect($test_base);
    if ($c === false) return 'CONNECT-FAIL';
    $q = @fbird_query($c, 'SELECT MON$FORCED_WRITES FROM MON$DATABASE');
    if (!$q) { fbird_close($c); return 'MON-FAIL'; }
    $r = fbird_fetch_row($q);
    fbird_free_result($q);
    fbird_close($c);
    return $r === false ? 'NO-ROW' : (string)$r[0];
}

$db_path = $test_base;
if (!empty($host) && strpos($test_base, $host . ':') === 0) {
    $db_path = substr($test_base, strlen($host) + 1);
}

// Phase 0: pristine baseline (init_db created the DB, no driver connect yet).
echo "--- pristine ---\n";
echo observables(svc_header($host, $user, $password, $db_path));

// Phase 1: procedural default connect.
$c = fbird_connect($test_base);
echo "--- after fbird_connect default ---\n";
echo observables(svc_header($host, $user, $password, $db_path));
fbird_close($c);
usleep(200000);

// Phase 2: persistent default connect.
$c = fbird_pconnect($test_base);
echo "--- after fbird_pconnect default ---\n";
echo observables(svc_header($host, $user, $password, $db_path));
fbird_close($c);
usleep(200000);

// Phase 3: OOP layer default connect.
$c = new Firebird\Connection($test_base, $user, $password);
echo "--- after Firebird\\Connection default ---\n";
echo observables(svc_header($host, $user, $password, $db_path));
unset($c);
usleep(200000);

// Phase 4: PDO layer default connect.
$pdo = new PDO('fbird:dbname=' . $test_base, $user, $password);
echo "--- after PDO fbird: default ---\n";
echo observables(svc_header($host, $user, $password, $db_path));
$pdo = null;
?>
--EXPECT--
--- pristine ---
page_buffers=0
db_dialect=3
attributes=force write
sweep_interval=absent
mon_forced_writes=1
--- after fbird_connect default ---
page_buffers=0
db_dialect=3
attributes=force write
sweep_interval=absent
mon_forced_writes=1
--- after fbird_pconnect default ---
page_buffers=0
db_dialect=3
attributes=force write
sweep_interval=absent
mon_forced_writes=1
--- after Firebird\Connection default ---
page_buffers=0
db_dialect=3
attributes=force write
sweep_interval=absent
mon_forced_writes=1
--- after PDO fbird: default ---
page_buffers=0
db_dialect=3
attributes=force write
sweep_interval=absent
mon_forced_writes=1
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

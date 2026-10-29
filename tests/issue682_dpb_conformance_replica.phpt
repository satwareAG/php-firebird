--TEST--
Issue #682: seeded dpb_set_db_replica violation flips MON$REPLICA_MODE, default connect preserves (FB4+)
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
/* Two capability gates, no version guessing:
 * 1. client: FBIRD_PRP_REPLICA_MODE only exists when the client lib headers
 *    had isc_spb_prp_replica_mode (FB4+). The php83-asan image ships an FB3
 *    client against the FB4 server - the property API cannot be driven, so
 *    the honest outcome is skip, not fail.
 * 2. server: probe MON$REPLICA_MODE directly (the FB4+ engine capability
 *    under test). A version-string regex would silently skip on unusual
 *    version strings and stay green - feature detection cannot
 *    (council round-1 objection S3). */
if (!defined('FBIRD_PRP_REPLICA_MODE')) {
    die('skip client lib lacks isc_spb_prp_replica_mode (FB3 client headers)');
}
$c = @fbird_connect($test_base);
if ($c === false) die('skip cannot connect');
$q = @fbird_query($c, 'SELECT MON$REPLICA_MODE FROM MON$DATABASE');
$has_replica_feature = ($q !== false);
if ($q) { fbird_free_result($q); }
fbird_close($c);
if (!$has_replica_feature) {
    die('skip server lacks MON$REPLICA_MODE (no replica feature)');
}
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

/*
 * Issue #682: negative control for dpb_set_db_replica (the 7th gated item).
 *
 * dpb_set_db_replica is FB4+ only, and the driver exposes no PHP API that
 * sends it - by design (src/cpp/fb_connection.hpp builds user/password/
 * charset/role/dialect/num_buffers/force_write/timezone/bind/workers only).
 * The seed channel is the service-manager property
 * isc_spb_prp_replica_mode (FBIRD_PRP_REPLICA_MODE), which drives the same
 * engine path (PAG_set_db_replica) the gated DPB item would.
 *
 * FB3 lanes never get here (SKIPIF), so this file's EXPECT is written for
 * FB4+ servers only.
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

function mon_replica(): string {
    global $test_base;
    $c = @fbird_connect($test_base);
    if ($c === false) return 'CONNECT-FAIL';
    $q = @fbird_query($c, 'SELECT MON$REPLICA_MODE FROM MON$DATABASE');
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

echo 'pristine=' . mon_replica() . "\n";
echo 'seed=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_REPLICA_MODE, FBIRD_PRP_RM_READONLY), true) . "\n";
echo 'flipped=' . mon_replica() . "\n";

$c = fbird_connect($test_base);
fbird_close($c);
usleep(200000);

echo 'after_default_connect=' . mon_replica() . "\n";
echo 'restore=' . var_export(svc_op($host, $user, $password, $db_path, FBIRD_PRP_REPLICA_MODE, FBIRD_PRP_RM_NONE), true) . "\n";
echo 'restored=' . mon_replica() . "\n";
?>
--EXPECT--
pristine=0
seed=true
flipped=1
after_default_connect=1
restore=true
restored=0
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

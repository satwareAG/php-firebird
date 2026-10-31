--TEST--
SEC FR-006 (#732): savepoint/gen_id identifier validation + version-aware length gate (31 on FB3, 63 on FB4+/FB5) - output is lane-independent
--CREDITS--
v13.4.0 security hardening train (#728) - FR-006 wire observer
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);
$trans = fbird_trans($link);

/* detect the deployed generation BEHAVIORALLY: probe whether a 32-byte
 * name is accepted (63-gate) or rejected (31-gate). The FB3-vs-FB5 divergence
 * proof lives in the named spec lanes (php84-fb3-dev / php84-fb5-dev); this
 * keeps the observer honest on mixed client/server lanes (e.g. the asan lane
 * ships an FB3 client against an FB4 server). */
$probe32 = 'sp_' . str_repeat('a', 29); /* exactly 32 bytes */
if (@fbird_savepoint($trans, $probe32) === true) {
    $limit = 63;
    fbird_release_savepoint($trans, $probe32);
} else {
    $limit = 31; /* 32-byte name rejected: deployed generation caps at 31 */
}

/* 1. invalid characters must be rejected with a module error (today: raw
 *    interpolation reaches the engine and produces a syntax error) */
$r = @fbird_savepoint($trans, 'sp; DROP TABLE x');
echo 'invalid-char savepoint rejected: ' . ($r === false ? 'yes' : 'NO') . "\n";
echo 'invalid-char error: ' . (strpos(fbird_errmsg(), 'Invalid savepoint name') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

/* 2. limit-aware gate: a name of exactly $limit bytes is accepted, $limit+1 rejected */
$ok_name = 'sp_' . str_repeat('a', $limit - 3);      /* exactly $limit bytes */
$r_ok = fbird_savepoint($trans, $ok_name);
echo 'exact-limit savepoint accepted: ' . ($r_ok === true ? 'yes' : 'NO: ' . fbird_errmsg()) . "\n";
$over_name = 'sp_' . str_repeat('a', $limit - 2);    /* $limit+1 bytes */
$r_over = @fbird_savepoint($trans, $over_name);
echo 'over-limit savepoint rejected: ' . ($r_over === false ? 'yes' : 'NO') . "\n";

/* 3. the savepoint actually exists (proves the acceptance was real) */
echo 'release savepoint: ' . (fbird_release_savepoint($trans, $ok_name) === true ? 'yes' : 'NO: ' . fbird_errmsg()) . "\n";

/* 4. gen_id: same treatment on the generator-name gate */
$gen_name = 'g_' . str_repeat('a', $limit - 2);      /* exactly $limit bytes */
fbird_query($link, 'CREATE GENERATOR ' . $gen_name);
$v = @fbird_gen_id($gen_name, 1, $link);
echo 'exact-limit gen_id accepted: ' . ($v == 1 ? 'yes' : 'NO: ' . fbird_errmsg()) . "\n";
$over_gen = 'g_' . str_repeat('a', $limit - 1);      /* $limit+1 bytes */
$v2 = @fbird_gen_id($over_gen, 1, $link);
echo 'over-limit gen_id rejected: ' . ($v2 === false ? 'yes' : 'NO') . "\n";
echo 'over-limit gen_id error: ' . (strpos(fbird_errmsg(), 'Invalid generator name') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

fbird_commit($trans);
fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
invalid-char savepoint rejected: yes
invalid-char error: ok
exact-limit savepoint accepted: yes
over-limit savepoint rejected: yes
release savepoint: yes
exact-limit gen_id accepted: yes
over-limit gen_id rejected: yes
over-limit gen_id error: ok
=== DONE ===

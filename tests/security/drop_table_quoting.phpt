--TEST--
SEC FR-002 (#730): _fbird_drop_table() must quote/validate identifiers - injection-shaped and comment-suffix names reject loudly, unquoted lowercase fold and quoted identifiers preserved (RDB$RELATIONS inventory is the observed surface)
--CREDITS--
v13.4.0 security hardening train (#728) - FR-002 wire observer
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);

fbird_query($link, 'CREATE TABLE safe_a (id INTEGER)');
fbird_query($link, 'CREATE TABLE safe_b (id INTEGER)');
fbird_query($link, 'CREATE TABLE "Mixed_Case" (id INTEGER)');
fbird_commit($link);

function inventory($link) {
    $out = [];
    $res = fbird_query($link, "SELECT TRIM(RDB\$RELATION_NAME) AS TBL FROM RDB\$RELATIONS WHERE RDB\$VIEW_BLR IS NULL AND (RDB\$SYSTEM_FLAG = 0 OR RDB\$SYSTEM_FLAG IS NULL) ORDER BY RDB\$RELATION_NAME");
    while ($row = fbird_fetch_assoc($res)) { $out[] = $row['TBL']; }
    fbird_free_result($res);
    return $out;
}

$before = inventory($link);
echo 'fixtures present: ' . (count(array_intersect(['SAFE_A', 'SAFE_B', 'Mixed_Case'], $before)) === 3 ? 'ok' : 'FAIL') . "\n";

/* (a) comment-suffix name: engine strips the comment and drops the table - must reject */
$r = @fbird_drop_table_force($link, 'SAFE_A -- junk');
echo 'comment-suffix rejected: ' . ($r === false ? 'yes' : 'NO - dropped silently') . "\n";
echo 'comment-suffix error: ' . (strpos(fbird_errmsg(), 'Invalid table name') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

/* (b) injection-shaped name: must reject before the engine sees it */
$r2 = @fbird_drop_table_force($link, 'SAFE_A; DROP TABLE SAFE_B');
echo 'injection rejected: ' . ($r2 === false ? 'yes' : 'NO') . "\n";
echo 'injection error: ' . (strpos(fbird_errmsg(), 'Invalid table name') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

/* the attacked relations must all still exist */
$after_attacks = inventory($link);
echo 'inventory intact after attacks: ' . ($after_attacks === $before ? 'ok' : 'FAIL - relation lost via raw interpolation') . "\n";

/* (c) doctrine contract: unquoted lowercase folds and drops the uppercase relation */
$r3 = fbird_drop_table_force($link, 'safe_b');
echo 'lowercase drop ok: ' . ($r3 === true ? 'yes' : 'NO: ' . fbird_errmsg()) . "\n";

/* (d) valid quoted identifier: drops the case-sensitive relation only */
$r4 = fbird_drop_table_force($link, '"Mixed_Case"');
echo 'quoted drop ok: ' . ($r4 === true ? 'yes' : 'NO: ' . fbird_errmsg()) . "\n";

/* final inventory: exactly $before minus the two intentionally dropped relations;
 * SAFE_A (the attacked relation) MUST have survived the attacks */
$after = inventory($link);
$expected_after = array_values(array_diff($before, ['SAFE_B', 'Mixed_Case']));
echo 'final inventory as intended: ' . ($after == $expected_after && in_array('SAFE_A', $after, true) ? 'ok' : 'FAIL: ' . implode(',', $after)) . "\n";

fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
fixtures present: ok
comment-suffix rejected: yes
comment-suffix error: ok
injection rejected: yes
injection error: ok
inventory intact after attacks: ok
lowercase drop ok: yes
quoted drop ok: yes
final inventory as intended: ok
=== DONE ===

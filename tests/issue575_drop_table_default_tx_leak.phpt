--TEST--
fbird_drop_table_force() must not orphan the default transaction struct (#575)
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
?>
--ENV--
; jane: detect_leaks=0 scopes the pre-existing orphaned-trans leak (#597,
; let-it-leak class); ASAN UAF detection stays armed.
ASAN_OPTIONS=detect_leaks=0
--FILE--
<?php
require_once('firebird.inc');

/* Issue #575: _fbird_drop_table() early-commits the transaction and clears
 * the default-tx tr_list slot, but never efree'd the fbird_transaction
 * struct itself - a per-call leak (the link-close commit path frees the
 * struct; the drop_table path did not). The struct must be detached from
 * its query/batch registries and efree'd, with the slot left reusable. */

$c = fbird_connect($test_base, $user, $password);
var_dump($c instanceof Firebird\Connection);

/* Two sequential force-drops through the DEFAULT transaction: the second
 * call exercises the slot-reuse path (_php_fbird_def_trans must safely
 * create a fresh default tx after the first drop consumed the struct). */
foreach ([1, 2] as $i) {
    $r = fbird_query($c, "RECREATE TABLE DROP575_$i (ID INTEGER)");
    var_dump($r === true);
    var_dump(fbird_drop_table_force($c, "DROP575_$i"));
}

/* The connection must stay fully usable afterwards. */
$r = fbird_query($c, 'SELECT COUNT(*) FROM RDB$DATABASE');
$row = fbird_fetch_row($r);
echo "still usable: " . (is_array($row) ? "yes" : "no") . "\n";

fbird_close($c);
echo "survived\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
still usable: yes
survived
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

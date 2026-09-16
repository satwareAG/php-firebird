--TEST--
Issue #624: idle release defers while sibling queries remain registered on the transaction
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// The doctrine 3.18.x window (#624): several statements share one explicit
// transaction. TM::autoCommit() fires fbird_commit_ret() - with cursors open
// this retains. When the results free afterwards, the #586 idle release must
// NOT hard-commit+restart while the prepared sibling statements are still
// alive (registered on the transaction, #594 registry): the restart
// invalidates their execution state and a later reuse observes truncated
// results. The release must defer until the last sibling query is freed.
//
// Observable: the transaction handle. A hard-commit+restart (#586) ends the
// transaction and starts a new one, so MON$TRANSACTION_ID (via
// CURRENT_TRANSACTION) changes. Deferral preserves the id; the deferred
// release at the last query free changes it.

function tx_id($trans): string {
    // executed ON the transaction: reports its own CURRENT_TRANSACTION id.
    // defensive: a dead transaction yields "dead" instead of a fatal so the
    // assertion diff stays readable.
    $q = @fbird_prepare_ex($GLOBALS['cA'],
        "SELECT MON\$TRANSACTION_ID FROM MON\$TRANSACTIONS WHERE MON\$TRANSACTION_ID = CURRENT_TRANSACTION", $trans);
    if (!$q) {
        return "dead";
    }
    $r = @fbird_execute($q);
    if (!$r) {
        fbird_free_query($q);
        return "dead";
    }
    $row = fbird_fetch_row($r);
    fbird_free_result($r);
    fbird_free_query($q);
    return $row === false ? "dead" : (string)$row[0];
}

// Setup: table with 5 committed rows via the default transaction
// (#572 autocommits the DDL; #294 commits DML only at SELECT result free,
// so the link commit below is what makes the rows visible).
$cA = fbird_connect($test_base);

$qx = @fbird_prepare_ex($cA, 'DROP TABLE T624_SIB', null);
if ($qx) { @fbird_execute($qx); fbird_free_query($qx); }
$qD = fbird_prepare_ex($cA, 'CREATE TABLE T624_SIB (id INTEGER)', null);
var_dump(fbird_execute($qD) === true);
fbird_free_query($qD);
for ($i = 1; $i <= 5; $i++) {
    $qI = fbird_prepare_ex($cA, "INSERT INTO T624_SIB VALUES ($i)", null);
    fbird_execute($qI);
    fbird_free_query($qI);
}
var_dump(fbird_commit($cA));

// Driver-shaped explicit transaction (TM defaults): READ COMMITTED + REC_VERSION
$tA = fbird_trans_start($cA, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_VERSION,
    'lock_resolution' => FBIRD_WAIT,
]);
var_dump($tA instanceof Firebird\Transaction || is_resource($tA));

$id1 = tx_id($tA);

// Two sibling SELECT statements on the SAME transaction, both executed -
// cursors open, both statements registered on the transaction.
$s1 = fbird_prepare_ex($cA, 'SELECT id FROM T624_SIB ORDER BY id', $tA);
$r1 = fbird_execute($s1);
$s2 = fbird_prepare_ex($cA, 'SELECT id FROM T624_SIB ORDER BY id DESC', $tA);
$r2 = fbird_execute($s2);
var_dump($r1 instanceof Firebird\ResultSet);
var_dump($r2 instanceof Firebird\ResultSet);

// Retaining commit WITH open cursors - sets retain_committed (#586).
var_dump(fbird_commit_ret($tA));

// Free the results: the last cursor closes. S1/S2 remain registered.
var_dump(fbird_free_result($r1));
var_dump(fbird_free_result($r2));

// DEFERRAL: with siblings registered the idle release must NOT have fired -
// the transaction must still be the SAME handle (same transaction id).
$id2 = tx_id($tA);
var_dump($id2 === $id1);

// Last sibling freed -> transaction observably idle -> the deferred release
// fires now: the transaction is restarted, id changes.
var_dump(fbird_free_query($s1));
var_dump(fbird_free_query($s2));
$id3 = tx_id($tA);
var_dump($id3 !== $id2);

// Post-restart sanity: the restarted transaction serves NEW statements
// correctly (restarted from the stored TPB, #586/#540 machinery).
$qNew = fbird_prepare_ex($cA, 'SELECT id FROM T624_SIB ORDER BY id', $tA);
$rNew = fbird_execute($qNew);
$n = 0;
while (($row = fbird_fetch_row($rNew)) !== false) {
    $n++;
}
var_dump($n === 5);
fbird_free_result($rNew);
fbird_free_query($qNew);

// cleanup
var_dump(fbird_commit($tA));
$qy = fbird_prepare_ex($cA, 'DROP TABLE T624_SIB', null);
fbird_execute($qy);
fbird_free_query($qy);

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

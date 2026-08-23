--TEST--
Issue #586: commit_ret() after DDL must not retain metadata relation locks
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// Mimic the doctrine-firebird-driver stack (issue #578/#586 evidence):
// TM->beginTransaction() -> fbird_prepare_ex($conn, DDL, activeTx)
// -> execute -> autoCommit() == fbird_commit_ret($tx).
//
// Pre-fix: isc_commit_retaining ends the transaction but RETAINS the
// relation locks (SW) on every DDL-touched system catalog relation at
// attachment level until disconnect. Any other attachment starting a
// SERIALIZABLE (consistency) READ-WRITE NO-WAIT transaction then fails
// immediately: "lock conflict on no wait transaction" (observed on
// RDB$RELATION_FIELDS in the #578 suite flake).

$cA = fbird_connect($test_base);
var_dump($cA instanceof Firebird\Connection);

// Driver-shaped TPB: READ COMMITTED + REC_VERSION, WRITE, WAIT (TM defaults)
$tA = fbird_trans_start($cA, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_VERSION,
    'lock_resolution' => FBIRD_WAIT,
]);
var_dump($tA instanceof Firebird\Transaction || is_resource($tA));

$qA = fbird_prepare_ex($cA, 'CREATE TABLE T586_LOCKS (id INTEGER)', $tA);
var_dump(is_resource($qA) || $qA instanceof Firebird\Statement);
var_dump(fbird_execute($qA) === true);
fbird_free_query($qA);

// THE LEAK SITE: retaining commit with zero open cursors.
// Nothing needs retention here - locks should be released.
var_dump(fbird_commit_ret($tA));

// Connection B (fresh physical attachment) runs a SERIALIZABLE
// READ-WRITE NO-WAIT transaction touching metadata - mirrors
// testSetIsolationLevelSerializable (#578).
$cB = fbird_connect($test_base, '', '', '', 0, 3, '', FBIRD_CONNECT_FORCE_NEW);
$tB = fbird_trans_start($cB, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_CONSISTENCY,
    'lock_resolution' => FBIRD_NOWAIT,
]);
$ok = false;
if ($tB) {
    $qB = @fbird_prepare_ex($cB, 'SELECT COUNT(*) FROM T586_LOCKS', $tB);
    if ($qB && fbird_execute($qB)) {
        $rB = fbird_fetch_row($qB);
        $ok = ($rB !== false && (int)$rB[0] === 0);
    }
    if ($qB) {
        fbird_free_query($qB);
    }
}
var_dump($ok);
if (!$ok) {
    echo "errmsg: " . trim(fbird_errmsg()) . "\n";
}

// Release B's legitimate degree-3 locks BEFORE A's DROP cleanup:
// a consistency tx holds relation locks until commit, and A's default-tx
// DDL runs in WAIT mode - wrong order would deadlock the test.
fbird_commit($tB);
fbird_close($cB);

// cleanup
$qC = fbird_prepare_ex($cA, 'DROP TABLE T586_LOCKS', null);
fbird_execute($qC);
fbird_free_query($qC);

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

--TEST--
Issue #586 (case 2): commit_ret WITH an open cursor releases locks when the cursor closes
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// Variant of ddl_commit_ret_locks_001: the retaining commit fires while a
// SELECT cursor is still OPEN on the transaction (the doctrine TM::autoCommit
// sequence for executeQuery: execute -> cursor opens -> autoCommit -> fetch).
// The retain is legitimate (cursor needs it), but once the LAST cursor
// closes there is nothing left to retain - the retained system-catalog SW
// locks must be released transparently (lazy release, _php_fbird_trans_release_if_idle).

$cA = fbird_connect($test_base);

$tA = fbird_trans_start($cA, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_VERSION,
    'lock_resolution' => FBIRD_WAIT,
]);

// DDL on the explicit tx (autocommit-style driver shape)
$qA = fbird_prepare_ex($cA, 'CREATE TABLE T586_C2 (id INTEGER)', $tA);
var_dump(fbird_execute($qA) === true);
fbird_free_query($qA);

// Open a cursor on the SAME transaction, then commit_ret WITH cursor open.
// fbird_execute() returns the child result query (the actual cursor holder,
// mirroring the driver's Result object) - fetch/free THAT.
$qS = fbird_prepare_ex($cA, 'SELECT COUNT(*) FROM RDB$RELATIONS', $tA);
var_dump(is_resource($qS) || $qS instanceof Firebird\Statement);
$rS = fbird_execute($qS);
var_dump((bool)$rS);

// THE cursor-holding retain: locks stay (cursor needs them) + flag set
var_dump(fbird_commit_ret($tA));

// Drain the cursor; when it closes, the lazy release must fire
$row = fbird_fetch_row($rS);
var_dump($row !== false);
while (fbird_fetch_row($rS)) {}
fbird_free_query($rS);
fbird_free_query($qS);

// Connection B: SERIALIZABLE READ-WRITE NO-WAIT metadata access must succeed
$cB = fbird_connect($test_base, '', '', '', 0, 3, '', FBIRD_CONNECT_FORCE_NEW);
$tB = fbird_trans_start($cB, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_CONSISTENCY,
    'lock_resolution' => FBIRD_NOWAIT,
]);
$ok = false;
if ($tB) {
    $qB = @fbird_prepare_ex($cB, 'SELECT COUNT(*) FROM T586_C2', $tB);
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

// Release B's legitimate degree-3 locks BEFORE A's DROP cleanup
fbird_commit($tB);
fbird_close($cB);

// cleanup
$qC = fbird_prepare_ex($cA, 'DROP TABLE T586_C2', null);
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

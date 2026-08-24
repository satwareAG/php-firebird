--TEST--
Issue #589: rollback_ret() must not retain metadata relation locks (#586 twin)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

/* Mirror of the #586 probe with isc_rollback_retaining: A executes DDL on
 * a READ COMMITTED/REC_VERSION tx then retains-rollback; B starts a fresh
 * SERIALIZABLE (CONSISTENCY) READ-WRITE NOWAIT transaction over metadata.
 * Pre-fix the retaining rollback kept attachment-level relation locks and
 * B failed immediately ("lock conflict on no wait transaction", #578
 * shape). Post-fix, a zero-cursor rollback_ret hard-rolls-back + restarts,
 * releasing everything - observably identical for the caller. */

$cA = fbird_connect($test_base);
var_dump($cA instanceof Firebird\Connection);

$tA = fbird_trans_start($cA, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_VERSION,
    'lock_resolution' => FBIRD_WAIT,
]);

$qA = fbird_prepare_ex($cA, 'CREATE TABLE T589_RR (id INTEGER)', $tA);
var_dump(fbird_execute($qA) === true);
fbird_free_query($qA);

// Retaining rollback undoes the transactional DDL...
var_dump(fbird_rollback_ret($tA));

$chk = fbird_query($cA, "SELECT COUNT(*) FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'T589_RR'");
$row = $chk ? fbird_fetch_row($chk) : false;
echo "table-gone: ";
var_dump($row ? ((int)$row[0]) === 0 : false);
if ($chk) fbird_free_result($chk);

// ...but must not keep the relation locks (the #589 defect).
$cB = fbird_connect($test_base, '', '', '', 0, 3, '', 0, FBIRD_CONNECT_FORCE_NEW);
$tB = fbird_trans_start($cB, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_CONSISTENCY,
    'lock_resolution' => FBIRD_NOWAIT,
]);
$ok = false;
if ($tB) {
    $qB = @fbird_prepare_ex($cB, "SELECT COUNT(*) FROM RDB\$RELATION_FIELDS", $tB);
    if ($qB && fbird_execute($qB)) {
        $rB = fbird_fetch_row($qB);
        $ok = ($rB !== false);
        if ($qB) fbird_free_query($qB);
    }
}
echo "serializable-nowait-after-rollback_retain: ";
var_dump($ok);

fbird_close($cA); fbird_close($cB);
echo "done\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
--EXPECT--
bool(true)
bool(true)
bool(true)
table-gone: bool(true)
serializable-nowait-after-rollback_retain: bool(true)
done

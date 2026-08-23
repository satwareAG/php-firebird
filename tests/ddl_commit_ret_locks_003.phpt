--TEST--
PR #588 review: releaseMetadataLocks() must clear retain_committed - stale flag silently commits later DML
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// Sequence (review finding on PR #588):
// 1. explicit tx + open SELECT cursor on it
// 2. fbird_commit_ret() with the cursor open -> retaining path sets
//    retain_committed = true
// 3. $tx->releaseMetadataLocks() -> hard commit + restart. The old
//    hand-rolled OOP copy never cleared retain_committed.
// 4. INSERT (uncommitted DML on the restarted tx)
// 5. free the cursor -> idle release fires on the stale flag and
//    transparently HARD-COMMITS the step-4 INSERT (the bug)
// 6. fbird_rollback() -> cannot undo an already committed insert
//
// With the fix (helper clears the flag) step 5 is a no-op and the
// rollback discards the INSERT: final count must be 0.

$c = fbird_connect($test_base);
var_dump($c instanceof Firebird\Connection);

// Table via default-tx DDL (#572: autocommits immediately)
$q = fbird_prepare_ex($c, 'CREATE TABLE T588_STALE (id INTEGER)', null);
var_dump(fbird_execute($q) === true);
fbird_free_query($q);

$tx = fbird_trans_start($c, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_VERSION,
    'lock_resolution' => FBIRD_WAIT,
]);
var_dump($tx instanceof Firebird\Transaction || is_resource($tx));

// 1. open SELECT cursor on the tx (execute returns a ResultSet for SELECT)
$qsel = fbird_prepare_ex($c, 'SELECT COUNT(*) FROM RDB$DATABASE', $tx);
var_dump((bool) fbird_execute($qsel));

// 2. retaining commit WITH the cursor open -> sets retain_committed
var_dump(fbird_commit_ret($tx));

// 3. OOP hard commit + restart on the SAME resource-backed tx
var_dump($tx->releaseMetadataLocks());

// 4. uncommitted DML on the restarted tx (execute returns affected-row count)
$qins = fbird_prepare_ex($c, 'INSERT INTO T588_STALE (id) VALUES (1)', $tx);
var_dump((bool) fbird_execute($qins));
fbird_free_query($qins);

// 5. last cursor closes -> idle release must NOT fire (flag cleared)
fbird_free_query($qsel);

// 6. rollback must still be able to discard the INSERT
var_dump(fbird_rollback($tx));

$qc = fbird_prepare_ex($c, 'SELECT COUNT(*) FROM T588_STALE', null);
fbird_execute($qc);
$row = fbird_fetch_row($qc);
var_dump((int)$row[0] === 0);
fbird_free_query($qc);

// cleanup
$qd = fbird_prepare_ex($c, 'DROP TABLE T588_STALE', null);
fbird_execute($qd);
fbird_free_query($qd);

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
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

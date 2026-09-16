--TEST--
fbird_commit_ret() on an explicit transaction whose handle was already ended must self-heal (driver#201)
--DESCRIPTION--
Doctrine TransactionManager keeps a cached $activeTransaction resource across
statements and calls fbird_commit_ret() on it repeatedly (driver#201). When the
extension ends the underlying handle behind that resource's back (absorbed
#586 restart failure, or hard commit while a second resource reference keeps
the resource alive), the next fbird_commit_ret() currently raises
"invalid transaction handle (expecting explicit transaction start)" - the exact
500 signature in driver#201 that healDeadTransaction() cannot match.

Contract: commit_ret on an ended-but-alive explicit transaction restarts the
transaction transparently (same philosophy as #586/#589 restart paths and the
#294 idempotent default-tx success) and reports success, leaving the
transaction usable.
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$db = fbird_connect($test_base);

$tr = fbird_trans($db);
fbird_query($tr, "RECREATE TABLE dr201_tx (id INTEGER)");
fbird_commit($tr);

$tr = fbird_trans($db);
fbird_query($tr, "INSERT INTO dr201_tx VALUES (1)");

/* Second zval reference: the resource survives fbird_commit()'s
 * zend_list_delete() - the live-resource + NULL-handle state. */
$keepalive = $tr;

/* Hard end: fbt_transaction = NULL, resource kept alive by $keepalive. */
fbird_commit($tr);

/* driver#201 call: commit_ret on the ended explicit transaction. */
if (fbird_commit_ret($tr)) {
    echo "commit_ret on ended explicit tx: OK\n";
} else {
    echo "commit_ret on ended explicit tx: FAILED\n";
}

/* Healed transaction must be usable again. */
if (fbird_query($tr, "INSERT INTO dr201_tx VALUES (2)")) {
    echo "insert on healed tx: OK\n";
    fbird_commit_ret($tr);
} else {
    echo "insert on healed tx: FAILED\n";
}

$res = fbird_query($tr, "SELECT id FROM dr201_tx ORDER BY id");
$n = 0;
while (fbird_fetch_row($res)) { $n++; }
echo "rows: $n\n";

fbird_rollback($tr);
fbird_close($db);
?>
--EXPECT--
commit_ret on ended explicit tx: OK
insert on healed tx: OK
rows: 2

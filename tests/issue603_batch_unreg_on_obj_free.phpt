--TEST--
Batch object free while its transaction stays alive must unregister from the trans registry (#603)
--SKIPIF--
<?php
require __DIR__ . '/skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API (fbird_batch_create) not available in this build');
}
if (get_fb_version() < 4.0) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--ENV--
; jane: detect_leaks=0 scopes the pre-existing orphaned-trans leak (#597,
; let-it-leak class); ASAN UAF detection stays armed.
ASAN_OPTIONS=detect_leaks=0
--FILE--
<?php
require_once('firebird.inc');

/* Issue #603: fbird_batch_create() returns a Firebird\BatchHandle object.
 * fbird_batch_free_obj() freed the fbird_batch struct WITHOUT unregistering
 * it from the owning transaction's batch registry, so trans->batch_head
 * kept a dangling pointer. While the #599 sweep walk in
 * _php_fbird_trans_detach_queries() was dormant (the #600 order bug), the
 * dangling entry was never dereferenced; once the walk activates, any
 * transaction whose batches died through the object path segfaults at
 * request shutdown (fbird_batch_multitype_001, process exit).
 *
 * This test chains several batch objects on ONE transaction, frees them
 * through the object path while the transaction stays alive, forces a new
 * enrollment onto the (post-fix empty) registry, and lets the transaction
 * dtor walk the registry at shutdown. Pre-fix (walk active, no unreg) this
 * is a use-after-free in the shutdown walk; post-fix it exits cleanly. */

$c = fbird_connect($test_base, $user, $password);
var_dump($c instanceof Firebird\Connection);

$r = fbird_query($c, 'RECREATE TABLE BATCH603 (ID INTEGER)');
if ($r !== true) {
    echo "recreate failed: " . fbird_errmsg() . "\n";
    exit;
}
fbird_commit($c);
echo "table ready\n";

$t = fbird_trans($c);
$q = fbird_prepare($t, 'INSERT INTO BATCH603 (ID) VALUES (?)');
if (!$q) {
    echo "prepare failed: " . fbird_errmsg() . "\n";
    exit;
}
/* b1 created, executed (exercises the close-after-execute free path). */
$b1 = fbird_batch_create($q, $t);
var_dump($b1 instanceof Firebird\BatchHandle);
fbird_batch_add($b1, 1);
$res = fbird_batch_execute($b1);
echo "b1 executed: " . (int)($res['success_count'] === 1) . "\n";

/* b2 chained on the same transaction registry; only one open batch per
 * statement is allowed, so b2 is created after b1 finished. b2 is freed
 * while never executed: exercises the cancel+close free path. */
$b2 = fbird_batch_create($q, $t);
fbird_batch_add($b2, 2);

/* Free both through the object path while the transaction lives. Pre-fix
 * both stay enrolled as dangling registry entries. */
unset($b1);
unset($b2);
echo "batches freed while trans alive\n";

/* Force a fresh enrollment: pre-fix this chain next-pointer copies a
 * dangling value from trans->batch_head. */
$b3 = fbird_batch_create($q, $t);
fbird_batch_add($b3, 3);
unset($b3);

/* The connection/transaction pair must stay fully usable. ID=1 lives in
 * the still-open $t, invisible to the connection's default trans. */
$r = fbird_query($c, 'SELECT COUNT(*) FROM BATCH603');
$row = fbird_fetch_row($r);
echo "rows visible before commit: " . $row[0] . "\n";

/* Commit proves the transaction handle survived the batch frees, and the
 * post-commit count proves the batch actually inserted through it. The
 * connection's default trans needs an explicit end first - otherwise its
 * pre-commit snapshot hides the row (#572 default-tx lifetime). */
fbird_commit($t);
fbird_commit($c);
$r = fbird_query($c, 'SELECT COUNT(*) FROM BATCH603');
$row = fbird_fetch_row($r);
echo "rows visible after commit: " . $row[0] . "\n";

fbird_close($c);
unset($q, $t);
echo "survived\n";
?>
--EXPECT--
bool(true)
table ready
bool(true)
b1 executed: 1
batches freed while trans alive
rows visible before commit: 0
rows visible after commit: 1
survived
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

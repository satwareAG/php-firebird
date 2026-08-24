--TEST--
Batch resource survives its transaction struct being efree'd (#599)
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

/* Issue #599: fbird_batch stores a raw trans backref that
 * fbird_batch_execute() dereferences. Pre-fix, when the transaction's
 * struct was efree'd while the batch resource was still alive (the link
 * close frees the default tx), the backref dangled - the same UAF class
 * #594 fixed for query resources. Post-fix the transaction registry
 * detaches batches too: execute() then fails cleanly through the
 * NULL-handle guard instead of reading freed memory.
 * Prepared on the LINK so the default tx struct dies with drop_db(). */
$c = fbird_connect($test_base, $user, $password);
var_dump($c instanceof Firebird\Connection);

$ddl = "RECREATE TABLE BATCH_594T (ID INTEGER)";
var_dump(fbird_query($c, $ddl) !== false);

$q = fbird_prepare($c, "INSERT INTO BATCH_594T (ID) VALUES (?)");
var_dump($q !== false);

$batch = fbird_batch_create($q);
var_dump($batch !== false);

/* Drop succeeds (no other attachment), but the tx STRUCT only dies when
 * the link does - destroy the Connection object so commit_link efrees the
 * default tx. The registry must detach the batch during that efree. */
var_dump(fbird_drop_db($c));
unset($c);

/* Batch execute must fail cleanly through the NULL-guard, never touch
 * freed memory. */
$r = @fbird_batch_execute($batch);
var_dump($r === false);

unset($q, $batch);
echo "survived\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
survived

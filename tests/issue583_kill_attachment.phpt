--TEST--
fbird_kill_attachment() must actually terminate the target attachment (#583)
--SKIPIF--
<?php require __DIR__ . '/skipif.inc'; ?>
--ENV--
; jane: detect_leaks=0 scopes the pre-existing orphaned-trans leak (#597,
; let-it-leak class); ASAN UAF detection stays armed.
ASAN_OPTIONS=detect_leaks=0
--FILE--
<?php
require_once('firebird.inc');

/* Issue #583: fbird_kill_attachment() reported success but the target
 * attachment survived on Firebird 3.0. Two bugs:
 * 1. The input message buffer hardcoded the legacy XSQLDA layout
 *    (null indicator at 0, value at 8), but the engine's
 *    IMessageMetadata for the DELETE reports null_off=8, data_off=0.
 *    The engine read its null flag from the middle of the value -
 *    nonzero = NULL - so the DELETE matched zero rows.
 * 2. The kill only takes effect when the deleting transaction commits;
 *    the function never committed. It now runs the DELETE on a
 *    dedicated transaction and commits it before returning. */

$c1 = fbird_connect($test_base, $user, $password, '', 0, 3, '', 0, FBIRD_CONNECT_FORCE_NEW);
$c2 = fbird_connect($test_base, $user, $password, '', 0, 3, '', 0, FBIRD_CONNECT_FORCE_NEW);
$c3 = fbird_connect($test_base, $user, $password, '', 0, 3, '', 0, FBIRD_CONNECT_FORCE_NEW);
var_dump($c1 instanceof Firebird\Connection);
var_dump($c2 instanceof Firebird\Connection);

$q = fbird_query($c1, 'SELECT CURRENT_CONNECTION FROM RDB$DATABASE');
$row = fbird_fetch_row($q);
$id = (int) $row[0];
echo "victim id > 0: ";
var_dump($id > 0);

var_dump(fbird_kill_attachment($c2, $id));

/* The witness sees committed state: the MON$ row must be gone. */
$gone = false;
for ($i = 0; $i < 10; $i++) {
    usleep(200000);
    $q3 = fbird_query($c3, "SELECT COUNT(*) FROM MON\$ATTACHMENTS WHERE MON\$ATTACHMENT_ID = $id");
    $row3 = fbird_fetch_row($q3);
    if ((int) $row3[0] === 0) {
        $gone = true;
        break;
    }
}
echo "row gone: ";
var_dump($gone);

/* The victim's next operation must fail. */
$r = @fbird_query($c1, 'SELECT 1 FROM RDB$DATABASE');
echo "victim query fails: ";
var_dump($r === false);

/* jane: the killed link must be closed explicitly - request-shutdown
 * cleanup of a server-killed attachment still crashes in the query-dtor
 * transaction release (fbt_free -> release on the dead interface;
 * transaction wrappers are not covered by the #591 statement sweep -
 * follow-up issue). */
fbird_close($c1);
fbird_close($c2);
fbird_close($c3);
echo "done\n";
?>
--EXPECTF--
bool(true)
bool(true)
victim id > 0: bool(true)
bool(true)
row gone: bool(true)
victim query fails: bool(true)
done

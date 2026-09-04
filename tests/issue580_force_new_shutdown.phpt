--TEST--
Two FBIRD_CONNECT_FORCE_NEW links to the same DB must survive request shutdown (#580)
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

/* Issue #580: with two live physical connections to the same database, one
 * via the DSN cache and one via FBIRD_CONNECT_FORCE_NEW, request shutdown
 * used to SIGSEGV while closing the second link: the wrapper's IAttachment
 * pointer pointed into a freed region (fbc_disconnect ->
 * Connection::detachNoThrow -> pingAttachment -> IAttachment::getInfo).
 * Explicit fbird_close() avoided it; shutdown-order cleanup did not. */

$c1 = fbird_connect($test_base, $user, $password);
var_dump($c1 instanceof Firebird\Connection);

/* flags is the 9th positional param (#595): db, user, pass, charset,
 * buffers, dialect, role, sync, flags. */
$c2 = fbird_connect($test_base, $user, $password, '', 0, 3, '', 0,
	FBIRD_CONNECT_FORCE_NEW);
var_dump($c2 instanceof Firebird\Connection);

$q = fbird_query($c2, 'SELECT 1 FROM RDB$DATABASE');
$row = fbird_fetch_row($q);
echo "c2 query: " . $row[0] . "\n";
$q2 = fbird_query($c1, 'SELECT 2 FROM RDB$DATABASE');
$row2 = fbird_fetch_row($q2);
echo "c1 query: " . $row2[0] . "\n";

/* Deliberately NO fbird_close(): both links must be torn down safely by
 * request shutdown (this was the crashing order). */
echo "shutdown pending\n";
?>
--EXPECT--
bool(true)
bool(true)
c2 query: 1
c1 query: 2
shutdown pending

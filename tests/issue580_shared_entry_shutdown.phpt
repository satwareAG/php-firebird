--TEST--
Two wrappers sharing one DSN-cached entry must survive request shutdown (#580/#579)
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

/* Issue #580: two live wrappers over ONE non-persistent DSN-cached link
 * (no FBIRD_CONNECT_FORCE_NEW), no explicit close. At request shutdown the
 * shared link is torn down while both wrappers' query resources still
 * exist; pre-#594 the query dtor dereferenced the already-freed default
 * transaction struct (_php_fbird_cursor_closed heap-use-after-free,
 * ASAN-verified on main @ 16cc483). The #594 detach fixed the shutdown
 * ordering; this test guards it. */

$c1 = fbird_connect($test_base, $user, $password);
$c2 = fbird_connect($test_base, $user, $password);
var_dump($c1 instanceof Firebird\Connection);
var_dump($c2 instanceof Firebird\Connection);

$q = fbird_query($c1, 'SELECT 1 FROM RDB$DATABASE');
$row = fbird_fetch_row($q);
$q2 = fbird_query($c2, 'SELECT 2 FROM RDB$DATABASE');
$row2 = fbird_fetch_row($q2);
echo "q1: " . $row[0] . " q2: " . $row2[0] . "\n";

/* Deliberately NO fbird_close(): shutdown must tear down the shared link
 * and both wrappers' queries in a safe order. */
echo "shutdown pending\n";
?>
--EXPECT--
bool(true)
bool(true)
q1: 1 q2: 2
shutdown pending

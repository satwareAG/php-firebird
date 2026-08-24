--TEST--
Failed fbird_drop_db() must leave the link safely closed, not dangling (#591 review finding 3)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/* A second FORCE_NEW attachment keeps the database in use, forcing the drop
 * to fail. fbc_drop_database() deletes the fb::Connection on EVERY exit path
 * (success or failure). Pre-fix, fbird_drop_db() nulled fb_link->fbc_connection
 * only on success: after a failed drop the link kept a freed pointer and any
 * later use through it was a heap-use-after-free (visible under ASAN).
 * Post-fix the pointer is nulled before the call, so the link behaves like a
 * closed connection: subsequent queries fail cleanly with no UAF. */
$c = fbird_connect($test_base, $user, $password);
var_dump($c instanceof Firebird\Connection);

/* jane: different charset breaks DSN-cache sharing -> independent attachment.
 * Deliberately NOT passing FBIRD_CONNECT_FORCE_NEW: flags is the 9th
 * positional param (#595) and debug builds fatal on 9-arg calls until the
 * arginfo arity is fixed. */
$holder = fbird_connect($test_base, $user, $password, 'UTF8');
var_dump($holder instanceof Firebird\Connection);

/* Must fail: the database is still in use by $holder. */
var_dump(@fbird_drop_db($c));

/* The failed-drop link must hit the NULL-handle guard ("closed" semantics),
 * never dereference freed memory. */
$q = @fbird_query($c, "SELECT 1 FROM RDB\$DATABASE");
var_dump($q === false);

fbird_close($holder);
unset($c, $holder);
echo "survived\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
survived

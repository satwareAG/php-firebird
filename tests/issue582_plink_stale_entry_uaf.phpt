--TEST--
fbird_pconnect() stale plink entry must not free fb_link under live wrappers (#582)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/* Two pconnect calls to the same DSN share the cached link wrapper
 * (le_index_ptr cache in EG(regular_list)). Both $c1 and $c2 reference
 * the same fbird_db_link struct owned by the EG(persistent_list) entry. */
$c1 = fbird_pconnect($test_base, $user, $password);
var_dump($c1 !== false);

$c2 = fbird_pconnect($test_base, $user, $password);
var_dump($c2 !== false);

/* Kill the server attachment without freeing fb_link: fbird_drop_db()
 * detaches + frees the C++ connection wrapper and sets
 * fb_link->fbc_connection = NULL, but leaves fb_link and the
 * persistent_list entry alive. */
var_dump(fbird_drop_db($c1));

/* Same-DSN pconnect walks the stale branch in _php_fbird_connect_link.
 * BUG (#582): it deleted the persistent_list entry, whose plist dtor
 * freed fb_link while the wrappers held by $c1/$c2 still point at it.
 * The attach below fails (database is gone), but the free already
 * happened - request shutdown then dereferences freed memory in
 * _php_fbird_commit_link (heap-use-after-free under ASAN). */
$c3 = @fbird_pconnect($test_base, $user, $password);
var_dump($c3 === false);

/* Dereference the possibly-freed struct through a surviving wrapper. */
var_dump(@fbird_query($c1, "SELECT 1 FROM RDB\$DATABASE") === false);

/* Wrapper destruction (unset / request end) runs the commit dtor over
 * fb_link - the primary UAF read site with the bug. */
unset($c1, $c2);
echo "survived\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
survived

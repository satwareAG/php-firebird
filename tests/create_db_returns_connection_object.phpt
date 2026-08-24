--TEST--
fbird_query(FBIRD_CREATE, ...) returns Firebird\Connection, not a raw resource (#306 follow-up)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/* Since #306 the arginfo mask for fbird_query() declares
 * MAY_BE_OBJECT|MAY_BE_LONG|MAY_BE_BOOL. The legacy FBIRD_CREATE branch
 * still returned a raw resource: release builds did not enforce the mask,
 * but debug/ASAN builds fatal on every test whose skipif includes
 * firebird.inc (init_db creates its DB through this path).
 * Phase C parity: the create result is a Firebird\Connection object,
 * exactly like fbird_connect(). */
$charset = ini_get('fbird.default_charset') ?: 'NONE';
$db_name = $test_base . '_createconn';
$sql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s' DEFAULT CHARACTER SET %s",
    $db_name, $user, $password, $charset);

$c = @fbird_query(FBIRD_CREATE, $sql);
var_dump($c instanceof Firebird\Connection);

/* The returned object must be usable like any connection handle. */
var_dump(fbird_close($c));

/* Cleanup: best-effort drop of the extra database. */
var_dump(@fbird_drop_db($db_name, $user, $password));
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
done

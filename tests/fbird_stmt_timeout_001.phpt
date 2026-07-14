--TEST--
feat: per-statement timeout via IStatement::getTimeout/setTimeout (#464)
--SKIPIF--
<?php
include("skipif.inc");
if (!function_exists('fbird_stmt_set_timeout')) die('skip per-statement timeout requires FB 4.0+');
skip_if_fb_lt(4.0);
?>
--FILE--
<?php
require("firebird.inc");
global $test_base;
$link = fbird_connect($test_base);

/* Prepare a statement and set a per-statement timeout */
$stmt = fbird_prepare($link, "SELECT 1 FROM rdb\$database");
if (!$stmt) {
    echo "FAIL prepare: " . fbird_errmsg() . "\n";
    exit(1);
}

/* Set timeout to 3000ms */
$ok = fbird_stmt_set_timeout($stmt, 3000);
echo "set_timeout(3000): " . ($ok ? "ok" : "fail") . "\n";

/* Read it back */
$ms = fbird_stmt_get_timeout($stmt);
echo "get_timeout: " . var_export($ms, true) . "\n";

/* Set to 0 (no timeout) */
fbird_stmt_set_timeout($stmt, 0);
$ms2 = fbird_stmt_get_timeout($stmt);
echo "after set(0): " . var_export($ms2, true) . "\n";

/* Test that the statement still executes fine */
$res = fbird_execute($stmt);
$row = fbird_fetch_row($res);
echo "execute result: " . var_export($row[0], true) . "\n";

fbird_free_query($stmt);
fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
set_timeout(3000): ok
get_timeout: 3000
after set(0): 0
execute result: 1
=== DONE ===

--TEST--
Query result resources outlive connection death: drop_db and disconnect with live result resources (#591)
--ENV--
; jane: detect_leaks=0 scopes the pre-existing orphaned-trans leak (#597,
; let-it-leak class); ASAN UAF detection stays armed.
ASAN_OPTIONS=detect_leaks=0
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/* Path 1 (drop_db): result resource stays alive across the drop. The RAII
 * attachment release kills the statement interfaces; the result dtor at
 * request shutdown must not touch them (pre-fix: SIGSEGV in
 * StatementWrapper::closeCursor, Termsig=11). */
$c = fbird_connect($test_base, $user, $password);
$r = fbird_query($c, "SELECT 1 FROM RDB\$DATABASE");
var_dump($r !== false);
var_dump(fbird_drop_db($c));

/* Recreate the database for path 2 (same statement shape firebird.inc uses). */
$charset = ini_get('fbird.default_charset') ?: 'NONE';
$sql = sprintf("CREATE DATABASE '%s' USER '%s' PASSWORD '%s' DEFAULT CHARACTER SET %s",
    $test_base, $user, $password, $charset);
try {
    $create = @fbird_query(FBIRD_CREATE, $sql);
} catch (Throwable $e) {
    $create = false;
}
var_dump($create !== false);
if ($create) {
    fbird_close($create);
}

/* Path 2 (disconnect): result resource stays alive across fbird_close().
 * fbc_disconnect releases the attachment; same class of dead interfaces. */
$c2 = fbird_connect($test_base, $user, $password);
$r2 = fbird_query($c2, "SELECT 1 FROM RDB\$DATABASE");
var_dump($r2 !== false);
fbird_close($c2);

unset($c, $c2, $r, $r2);
echo "survived\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
survived

--TEST--
fbird_pconnect() stale plink revitalization: reattach in place after db recreate (#582)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$c1 = fbird_pconnect($test_base, $user, $password);
var_dump($c1 !== false);
var_dump(fbird_query($c1, "SELECT 1 FROM RDB\$DATABASE") !== false);

/* Kill the attachment: fb_link survives with fbc_connection == NULL. */
var_dump(fbird_drop_db($c1));

/* Recreate the database file (same statement shape firebird.inc uses). */
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

/* Same-DSN pconnect hits the stale branch and must revitalize the existing
 * fb_link in place (new attachment, same struct) instead of freeing it. */
$c3 = fbird_pconnect($test_base, $user, $password);
var_dump($c3 !== false);

/* The pre-existing wrapper shares the revitalized fb_link and is usable
 * again - no use-after-free, no defunct state. */
var_dump(fbird_query($c1, "SELECT 1 FROM RDB\$DATABASE") !== false);

/* And the new wrapper works too. */
$r = fbird_query($c3, "SELECT 1 FROM RDB\$DATABASE");
var_dump($r !== false);
if ($r) {
    fbird_free_result($r);
}

unset($c1, $c3);
echo "survived\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
survived

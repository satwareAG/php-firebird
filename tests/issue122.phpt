--TEST--
Issue #122: fbird_drop_db() with connection string overload
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Testing fbird_drop_db() with connection string...\n";

$dsn = dirname($test_base) . "/issue122_" . getmypid() . ".fdb";

/* Create DB using new function */
$conn = fbird_create_database($dsn, $user, $password);
if (!$conn) {
    die("FAIL: cannot create: " . fbird_errmsg() . "\n");
}
fbird_close($conn);
echo "created\n";

/* Drop using string overload */
$result = fbird_drop_db($dsn, $user, $password);
var_dump($result);
echo "Done\n";
?>
--EXPECTF--
Testing fbird_drop_db() with connection string...
created
bool(true)
Done

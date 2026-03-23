--TEST--
Issue #128: fbird_execute() statement reuse
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);

echo "Preparing statement...\n";
$prep = fbird_prepare($conn, "INSERT INTO test1 (i, c) VALUES (?, ?)");

echo "Executing first time...\n";
$res1 = fbird_execute($prep, 101, 'First');
if ($res1) echo "OK: First execution worked\n";

echo "Executing second time (reuse)...\n";
$res2 = fbird_execute($prep, 102, 'Second');
if ($res2) echo "OK: Second execution worked\n";

fbird_free_query($prep);
fbird_close($conn);
?>
--EXPECTF--
Preparing statement...
Executing first time...
OK: First execution worked
Executing second time (reuse)...
OK: Second execution worked

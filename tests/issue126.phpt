--TEST--
Issue #126: Standardize fbird_prepare() and fbird_trans() signatures
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);

echo "Testing fbird_trans() with various argument combinations...\n";
// Current way: fbird_trans([resource link, [int options]])
$tr1 = fbird_trans($conn, FBIRD_WRITE);
if ($tr1) {
    echo "OK: fbird_trans(link, options)\n";
    fbird_rollback($tr1);
}

// Current way: fbird_trans(int options, [resource link]) - ARGUMENT SHIFTING
$tr2 = fbird_trans(FBIRD_WRITE, $conn);
if ($tr2) {
    echo "OK: fbird_trans(options, link)\n";
    fbird_rollback($tr2);
}

echo "\nTesting fbird_prepare() with various argument combinations...\n";
// Current way: fbird_prepare([resource link, [resource trans,]] string query)
$query = "SELECT * FROM RDB\$DATABASE";

$prep1 = fbird_prepare($conn, $query);
if ($prep1) {
    echo "OK: fbird_prepare(link, query)\n";
    fbird_free_query($prep1);
}

$prep2 = fbird_prepare($query);
if ($prep2) {
    echo "OK: fbird_prepare(query)\n";
    fbird_free_query($prep2);
}

fbird_close($conn);
?>
--EXPECTF--
Testing fbird_trans() with various argument combinations...
OK: fbird_trans(link, options)
OK: fbird_trans(options, link)

Testing fbird_prepare() with various argument combinations...
OK: fbird_prepare(link, query)
OK: fbird_prepare(query)

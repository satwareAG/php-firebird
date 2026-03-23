--TEST--
Issue #124: fbird_connection_info() procedural API proposal
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);

echo "Checking if fbird_connection_info exists in procedural API...\n";
if (function_exists('fbird_connection_info')) {
    echo "OK: fbird_connection_info exists\n";
    $info = fbird_connection_info($conn);
    var_dump($info);
} else {
    echo "Current behavior: fbird_connection_info does NOT exist in procedural API\n";
    echo "Note: It only exists as a method in Firebird\Connection class in v8\n";
}

fbird_close($conn);
?>
--EXPECTF--
Checking if fbird_connection_info exists in procedural API...
OK: fbird_connection_info exists
array(%d) {
%A}

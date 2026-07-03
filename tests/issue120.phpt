--TEST--
Issue #120: fbird_connect() return type (Proposal for Connection Object)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Connecting with fbird_connect()...\n";
$conn = fbird_connect($test_base, $user, $password);

if ($conn instanceof \Firebird\Connection) {
    echo "OOP behavior: Result is Firebird\\Connection object\n";
} elseif (is_resource($conn)) {
    echo "Legacy behavior: Result is a resource\n";
} elseif (is_object($conn)) {
    echo "Unknown object: " . get_class($conn) . "\n";
} else {
    echo "Unknown type: " . gettype($conn) . "\n";
}

fbird_close($conn);
?>
--EXPECTF--
Connecting with fbird_connect()...
OOP behavior: Result is Firebird\Connection object

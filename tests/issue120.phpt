--TEST--
Issue #120: fbird_connect() return type (Proposal for Connection Object)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Connecting with fbird_connect()...\n";
$conn = fbird_connect($test_base, $user, $password);

if (is_resource($conn)) {
    echo "Current behavior: Result is a resource\n";
} elseif (is_object($conn)) {
    echo "Proposed behavior: Result is an object (" . get_class($conn) . ")\n";
} else {
    echo "Unknown type: " . gettype($conn) . "\n";
}

fbird_close($conn);
?>
--EXPECTF--
Connecting with fbird_connect()...
Proposed behavior: Result is an object (Firebird\Connection)

--TEST--
Issue #123: Exception mode default in v9 (Proposal)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

echo "Connecting with fbird_connect()...\n";
$conn = fbird_connect($test_base, $user, $password);

echo "Attempting to execute invalid SQL WITHOUT manually calling fbird_set_exception_mode()...\n";
try {
    @fbird_query($conn, "INVALID SQL");
    echo "Current behavior: No exception thrown by default\n";
} catch (\Exception $e) {
    echo "Proposed behavior: Exception thrown by default (" . $e->getMessage() . ")\n";
}

fbird_close($conn);
?>
--EXPECTF--
Connecting with fbird_connect()...
Attempting to execute invalid SQL WITHOUT manually calling fbird_set_exception_mode()...
Current behavior: No exception thrown by default

--TEST--
Firebird\Connection: typed connection object via OOP layer (Issue #120)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

// Test 1: new Firebird\Connection(...) creates a typed object
$conn = new Firebird\Connection($test_base, "SYSDBA", "masterkey");
var_dump($conn instanceof Firebird\Connection);
echo get_class($conn) . "\n";

// Test 2: isConnected() returns true
var_dump($conn->isConnected());

// Test 3: beginTransaction() returns Firebird\Transaction
$tx = $conn->beginTransaction();
var_dump($tx instanceof Firebird\Transaction);

// Test 4: close works
$conn->close();
var_dump($conn->isConnected());

echo "Done\n";
?>
--EXPECT--
bool(true)
Firebird\Connection
bool(true)
bool(true)
bool(false)
Done

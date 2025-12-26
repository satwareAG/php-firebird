--TEST--
fbird_sqlstate() - SQLSTATE error code retrieval
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");

// Test 1: No error - should return false
echo "Test 1: No error state\n";
var_dump(fbird_sqlstate());

// Test 2: Connect and verify no error after successful connection
$db = fbird_connect($test_base);
if (!$db) {
    die("Failed to connect: " . fbird_errmsg());
}

echo "Test 2: After successful connection\n";
var_dump(fbird_sqlstate());

// Test 3: Cause a syntax error
echo "Test 3: Causing syntax error\n";
@fbird_query($db, "INVALID SQL SYNTAX HERE");
$state = fbird_sqlstate();
echo "SQLSTATE type: " . gettype($state) . "\n";
if (is_string($state)) {
    echo "SQLSTATE length: " . strlen($state) . "\n";
    echo "SQLSTATE starts with 42 (syntax error): " . (str_starts_with($state, '42') ? 'yes' : 'no') . "\n";
}

// Test 4: After successful query, state might still be set
echo "Test 4: After successful query\n";
fbird_query($db, "SELECT 1 FROM RDB\$DATABASE");
// Note: Firebird doesn't clear the error state after successful queries

fbird_close($db);

echo "Test complete\n";
?>
--EXPECTF--
Test 1: No error state
bool(false)
Test 2: After successful connection
bool(false)
Test 3: Causing syntax error
SQLSTATE type: string
SQLSTATE length: 5
SQLSTATE starts with 42 (syntax error): yes
Test 4: After successful query
Test complete

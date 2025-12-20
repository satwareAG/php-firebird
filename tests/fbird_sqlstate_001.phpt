--TEST--
fbird_sqlstate() - SQLSTATE error code retrieval
--EXTENSIONS--
firebird
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
require_once 'functions.inc';

// Test 1: No error - should return false
echo "Test 1: No error state\n";
var_dump(fbird_sqlstate());

// Test 2: Connect and cause an error
$db = setup_db_and_connect(__FILE__);

// Test 3: After successful connection - should have no error
echo "Test 2: After successful connection\n";
var_dump(fbird_sqlstate());

// Test 4: Cause a syntax error
echo "Test 3: Causing syntax error\n";
@fbird_query($db, "INVALID SQL SYNTAX HERE");
$state = fbird_sqlstate();
echo "SQLSTATE type: " . gettype($state) . "\n";
if (is_string($state)) {
    echo "SQLSTATE length: " . strlen($state) . "\n";
    echo "SQLSTATE starts with 42 (syntax error): " . (str_starts_with($state, '42') ? 'yes' : 'no') . "\n";
}

// Test 5: Clear state by making successful query
echo "Test 4: After successful query, state might still be set\n";
fbird_query($db, "SELECT 1 FROM RDB\$DATABASE");
// Note: Firebird doesn't clear the error state after successful queries
// The state persists until the next error or connection close

// Test 6: Cause integrity constraint violation
echo "Test 5: Causing NOT NULL constraint violation\n";
@fbird_query($db, "CREATE TABLE test_sqlstate (id INTEGER NOT NULL)");
@fbird_query($db, "INSERT INTO test_sqlstate (id) VALUES (NULL)");
$state = fbird_sqlstate();
echo "SQLSTATE type: " . gettype($state) . "\n";
if (is_string($state)) {
    echo "SQLSTATE length: " . strlen($state) . "\n";
    // 23xxx class is for integrity constraint violations
    echo "SQLSTATE class: " . substr($state, 0, 2) . "\n";
}

// Cleanup
@fbird_query($db, "DROP TABLE test_sqlstate");
fbird_close($db);
cleanup_db(__FILE__);

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
Test 4: After successful query, state might still be set
Test 5: Causing NOT NULL constraint violation
SQLSTATE type: string
SQLSTATE length: 5
SQLSTATE class: %s
Test complete

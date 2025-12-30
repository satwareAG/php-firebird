--TEST--
fbird_escape_string() - Escape strings for SQL queries
--EXTENSIONS--
firebird
--FILE--
<?php
// Test 1: String with single quote
$input1 = "O'Reilly";
$result1 = fbird_escape_string($input1);
echo "Test 1 - Single quote:\n";
echo "  Input:  $input1\n";
echo "  Output: $result1\n";
var_dump($result1 === "O''Reilly");

// Test 2: String without quotes (should be unchanged)
$input2 = "Hello World";
$result2 = fbird_escape_string($input2);
echo "\nTest 2 - No quotes:\n";
echo "  Input:  $input2\n";
echo "  Output: $result2\n";
var_dump($result2 === "Hello World");

// Test 3: Empty string
$input3 = "";
$result3 = fbird_escape_string($input3);
echo "\nTest 3 - Empty string:\n";
echo "  Input:  (empty)\n";
echo "  Output: (empty)\n";
var_dump($result3 === "");

// Test 4: Multiple single quotes
$input4 = "It's John's book";
$result4 = fbird_escape_string($input4);
echo "\nTest 4 - Multiple quotes:\n";
echo "  Input:  $input4\n";
echo "  Output: $result4\n";
var_dump($result4 === "It''s John''s book");

// Test 5: String starting with quote
$input5 = "'quoted'";
$result5 = fbird_escape_string($input5);
echo "\nTest 5 - Quotes at start and end:\n";
echo "  Input:  $input5\n";
echo "  Output: $result5\n";
var_dump($result5 === "''quoted''");

// Test 6: Consecutive quotes
$input6 = "a''b";
$result6 = fbird_escape_string($input6);
echo "\nTest 6 - Consecutive quotes (already escaped):\n";
echo "  Input:  $input6\n";
echo "  Output: $result6\n";
var_dump($result6 === "a''''b");

// Test 7: Only quotes
$input7 = "'''";
$result7 = fbird_escape_string($input7);
echo "\nTest 7 - Only quotes:\n";
echo "  Input:  $input7\n";
echo "  Output: $result7\n";
var_dump($result7 === "''''''");

// Test 8: Backslash is NOT escaped (Firebird does not use backslash escaping)
$input8 = "path\\to\\file";
$result8 = fbird_escape_string($input8);
echo "\nTest 8 - Backslash NOT escaped:\n";
echo "  Input:  $input8\n";
echo "  Output: $result8\n";
var_dump($result8 === "path\\to\\file");

// Test 9: Unicode string with quotes
$input9 = "Müller's café";
$result9 = fbird_escape_string($input9);
echo "\nTest 9 - Unicode with quote:\n";
echo "  Input:  $input9\n";
echo "  Output: $result9\n";
var_dump($result9 === "Müller''s café");

// Test 10: Return type is string
echo "\nTest 10 - Return type:\n";
var_dump(is_string(fbird_escape_string("test")));

echo "\nAll tests completed.\n";
?>
--EXPECT--
Test 1 - Single quote:
  Input:  O'Reilly
  Output: O''Reilly
bool(true)

Test 2 - No quotes:
  Input:  Hello World
  Output: Hello World
bool(true)

Test 3 - Empty string:
  Input:  (empty)
  Output: (empty)
bool(true)

Test 4 - Multiple quotes:
  Input:  It's John's book
  Output: It''s John''s book
bool(true)

Test 5 - Quotes at start and end:
  Input:  'quoted'
  Output: ''quoted''
bool(true)

Test 6 - Consecutive quotes (already escaped):
  Input:  a''b
  Output: a''''b
bool(true)

Test 7 - Only quotes:
  Input:  '''
  Output: ''''''
bool(true)

Test 8 - Backslash NOT escaped:
  Input:  path\to\file
  Output: path\to\file
bool(true)

Test 9 - Unicode with quote:
  Input:  Müller's café
  Output: Müller''s café
bool(true)

Test 10 - Return type:
bool(true)

All tests completed.
--TEST--
Coverage: fbird_gen_id — increments, error paths (long name, invalid chars)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die("connect failed: " . fbird_errmsg());

// Create a generator for testing
fbird_query($dbh, 'CREATE SEQUENCE COV_GEN_TEST START WITH 100');
fbird_commit($dbh);

// Test 1: gen_id with default increment (1)
echo "Test 1: gen_id default increment\n";
$val = fbird_gen_id('COV_GEN_TEST', 1, $dbh);
var_dump(is_int($val) || is_float($val));
var_dump($val >= 100);

// Test 2: gen_id with increment > 1
echo "Test 2: gen_id increment 10\n";
$val2 = fbird_gen_id('COV_GEN_TEST', 10, $dbh);
var_dump($val2 > $val);

// Test 3: gen_id with increment 0 (query current value without advancing)
echo "Test 3: gen_id increment 0 (peek)\n";
$val3 = fbird_gen_id('COV_GEN_TEST', 0, $dbh);
var_dump($val3 !== false);

// Test 4: gen_id with negative increment (countdown)
echo "Test 4: gen_id negative increment\n";
$val4 = fbird_gen_id('COV_GEN_TEST', -1, $dbh);
var_dump($val4 !== false);

// Test 5: gen_id error — name too long (> 31 chars)
echo "Test 5: gen_id name too long\n";
$r = @fbird_gen_id('ABCDEFGHIJKLMNOPQRSTUVWXYZ012345', 1, $dbh); // 32 chars
var_dump($r === false);

// Test 6: gen_id error — invalid chars in name
echo "Test 6: gen_id invalid chars in name\n";
$r = @fbird_gen_id("bad name!", 1, $dbh);
var_dump($r === false);

// Test 7: gen_id error — generator does not exist
echo "Test 7: gen_id nonexistent generator\n";
$r = @fbird_gen_id('COV_GEN_NONEXIST', 1, $dbh);
var_dump($r === false);

// Test 8: gen_id via default connection (no link arg)
echo "Test 8: gen_id via default connection\n";
// Set the connection as default and call without link arg
$val8 = fbird_gen_id('COV_GEN_TEST', 1);
var_dump($val8 !== false);

// Cleanup
fbird_query($dbh, 'DROP SEQUENCE COV_GEN_TEST');
fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECT--
Test 1: gen_id default increment
bool(true)
bool(true)
Test 2: gen_id increment 10
bool(true)
Test 3: gen_id increment 0 (peek)
bool(true)
Test 4: gen_id negative increment
bool(true)
Test 5: gen_id name too long
bool(true)
Test 6: gen_id invalid chars in name
bool(true)
Test 7: gen_id nonexistent generator
bool(true)
Test 8: gen_id via default connection
bool(true)
Done

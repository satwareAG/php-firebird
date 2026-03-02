--TEST--
Coverage: Parameter binding error paths — param count mismatch, bad prepared stmt, type errors
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip connect failed: ' . fbird_errmsg());

fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BIND_ERR_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BIND_ERR_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, 'CREATE TABLE BIND_ERR_COV (ID INTEGER NOT NULL PRIMARY KEY, V VARCHAR(20))');
fbird_commit($dbh);

$stmt = fbird_prepare($dbh, 'INSERT INTO BIND_ERR_COV (ID, V) VALUES (?, ?)');

// Test 1: too many parameters (3 bound, 2 expected)
echo "Test 1: too many parameters\n";
$r = @fbird_execute($stmt, 1, 'hello', 'extra');
if ($r === false) {
    echo "Rejected with error\n";
} else {
    echo "Executed (extra args ignored)\n";
}
fbird_commit($dbh);

// Test 2: too few parameters (1 bound, 2 expected)
echo "Test 2: too few parameters\n";
$r = @fbird_execute($stmt, 2);
if ($r === false) {
    echo "Rejected with error\n";
} else {
    echo "Executed (missing arg)\n";
}
fbird_commit($dbh);

// Test 3: fbird_execute with false (not a prepared statement resource)
echo "Test 3: execute with false handle\n";
$r = @fbird_execute(false, 1, 'x');
var_dump($r === false);

// Test 4: fbird_execute with connection resource (wrong type)
echo "Test 4: execute with connection resource\n";
$r = @fbird_execute($dbh, 1, 'x');
var_dump($r === false);

// Test 5: CHAR truncation — value longer than column allows
echo "Test 5: VARCHAR truncation (value longer than VARCHAR(20))\n";
$long = str_repeat('X', 100); // 100 chars into VARCHAR(20)
$r = @fbird_execute($stmt, 5, $long);
if ($r === false) {
    $err = fbird_errmsg();
    echo "Rejected: " . (strlen($err) > 0 ? "with error" : "unknown") . "\n";
} else {
    // Firebird may truncate silently or raise an error
    echo "Accepted (may be truncated)\n";
    fbird_commit($dbh);
}

// Test 6: fbird_num_params on valid and invalid statements
echo "Test 6: fbird_num_params on valid stmt\n";
$n = fbird_num_params($stmt);
var_dump((int)$n === 2);

echo "Test 7: fbird_num_params on false\n";
$n = @fbird_num_params(false);
var_dump($n === false || $n === null || $n === 0);

// Test 8: fbird_param_info on invalid statement
echo "Test 8: fbird_param_info on false\n";
$info = @fbird_param_info(false, 0);
var_dump($info === false || $info === null);

// Test 9: fbird_param_info out-of-range index on valid stmt
echo "Test 9: fbird_param_info out-of-range index\n";
$info = @fbird_param_info($stmt, 99);
var_dump($info === false || $info === null);

// Cleanup
fbird_query($dbh, 'DROP TABLE BIND_ERR_COV');
fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECTF--
Test 1: too many parameters
%s
Test 2: too few parameters
%s
Test 3: execute with false handle
bool(true)
Test 4: execute with connection resource
bool(true)
Test 5: VARCHAR truncation (value longer than VARCHAR(20))
%s
Test 6: fbird_num_params on valid stmt
bool(true)
Test 7: fbird_num_params on false
bool(true)
Test 8: fbird_param_info on false
bool(true)
Test 9: fbird_param_info out-of-range index
bool(true)
Done

--TEST--
fbird_trans_start accepts null options (reverse mismatch, Issue #307)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #307 (additional finding):
 *   fbird_trans_start arginfo says IS_ARRAY,1 (nullable) but C parser
 *   uses "a" (non-nullable). If null is passed, zend_parse_parameters
 *   rejects it despite the stub saying ?array.
 *
 * Fix: change "|za" -> "|za!" and add Z_TYPE_P guard
 */

$conn = fbird_connect($test_base);
if (!$conn) die("connect failed\n");

// Test 1: fbird_trans_start with null options (should work, not reject)
$tx = @fbird_trans_start($conn, null);
echo "trans_start null options: " . ($tx !== false ? "ok" : "failed") . "\n";
if ($tx) {
    fbird_commit($tx);
}

// Test 2: fbird_trans_start with array options (regression check)
$tx = fbird_trans_start($conn, [FBIRD_READ, FBIRD_COMMITTED]);
echo "trans_start array options: " . ($tx !== false ? "ok" : "failed") . "\n";
if ($tx) {
    fbird_commit($tx);
}

// Test 3: fbird_trans_start with no options (regression check)
$tx = fbird_trans_start($conn);
echo "trans_start no options: " . ($tx !== false ? "ok" : "failed") . "\n";
if ($tx) {
    fbird_commit($tx);
}

// Test 4: Reflection - 2nd param should be nullable
$rf = new ReflectionFunction('fbird_trans_start');
$params = $rf->getParameters();
if (count($params) >= 2) {
    $p2 = $params[1];
    echo "param1 allowsNull: ";
    var_dump($p2->allowsNull());
}

fbird_close($conn);
echo "Done\n";
?>
--EXPECTF--
trans_start null options: ok
trans_start array options: ok
trans_start no options: ok
param1 allowsNull: bool(true)
Done

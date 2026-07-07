--TEST--
fbird_get_client_version() returns float, not string (Issue #308)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #308:
 *   fbird_get_client_version() returns double via RETURN_DOUBLE,
 *   but arginfo declares IS_STRING and stub declares string.
 *
 * Fix: arginfo -> IS_DOUBLE, stub -> float
 */

// Test 1: Runtime return type is float/double
$ver = fbird_get_client_version();
echo "gettype: " . gettype($ver) . "\n";
var_dump(is_float($ver));

// Test 2: ReflectionFunction return type is float
$rf = new ReflectionFunction('fbird_get_client_version');
$rt = $rf->getReturnType();
echo "reflection type: " . $rt->getName() . "\n";
var_dump($rt->getName() === 'float');

// Test 3: Value matches major + minor / 10
$major = fbird_get_client_major_version();
$minor = fbird_get_client_minor_version();
var_dump($ver === (float)$major + (float)$minor / 10.0);

echo "Done\n";
?>
--EXPECT--
gettype: double
bool(true)
reflection type: float
bool(true)
bool(true)
Done

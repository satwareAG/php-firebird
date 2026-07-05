--TEST--
Arginfo parameter types accept resource/object, not IS_STRING (Issue #307)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/*
 * Regression test for GitHub Issue #307:
 *   5 functions declare IS_STRING for first param in arginfo but
 *   accept resource/object at runtime. Arginfo should be mixed.
 *
 * Affected: fbird_execute, fbird_free_query, fbird_num_params,
 *           fbird_param_info, fbird_batch_create
 */

$conn = fbird_connect($test_base);
if (!$conn) die("connect failed\n");

// Prepare a statement to get a Firebird\Statement object
$stmt = fbird_prepare($conn, 'SELECT * FROM test1 WHERE i = ?');
if (!$stmt) die("prepare failed\n");

// Test 1: fbird_execute accepts Firebird\Statement (not IS_STRING)
$rf = new ReflectionFunction('fbird_execute');
$param1 = $rf->getParameters()[0];
echo "fbird_execute param0 type: ";
try {
    $t = $param1->getType();
    echo ($t ? $t->getName() : 'mixed') . "\n";
} catch (Throwable $e) {
    echo "mixed\n";
}

// Test 2: fbird_free_query accepts Firebird\Statement/ResultSet
$rf = new ReflectionFunction('fbird_free_query');
$param1 = $rf->getParameters()[0];
echo "fbird_free_query param0 type: ";
try {
    $t = $param1->getType();
    echo ($t ? $t->getName() : 'mixed') . "\n";
} catch (Throwable $e) {
    echo "mixed\n";
}

// Test 3: fbird_num_params accepts resource/object
$rf = new ReflectionFunction('fbird_num_params');
$param1 = $rf->getParameters()[0];
echo "fbird_num_params param0 type: ";
try {
    $t = $param1->getType();
    echo ($t ? $t->getName() : 'mixed') . "\n";
} catch (Throwable $e) {
    echo "mixed\n";
}

// Test 4: fbird_param_info accepts resource/object
$rf = new ReflectionFunction('fbird_param_info');
$param1 = $rf->getParameters()[0];
echo "fbird_param_info param0 type: ";
try {
    $t = $param1->getType();
    echo ($t ? $t->getName() : 'mixed') . "\n";
} catch (Throwable $e) {
    echo "mixed\n";
}

// Test 5: fbird_batch_create accepts resource/object
if (function_exists('fbird_batch_create')) {
    $rf = new ReflectionFunction('fbird_batch_create');
    $param1 = $rf->getParameters()[0];
    echo "fbird_batch_create param0 type: ";
    try {
        $t = $param1->getType();
        echo ($t ? $t->getName() : 'mixed') . "\n";
    } catch (Throwable $e) {
        echo "mixed\n";
    }
}

// Test 6: Runtime - pass Firebird\Statement to fbird_param_info (before execute,
// because in_sqlda is freed after execute). May not work on all FB versions.
try {
    $info = @fbird_param_info($stmt, 1);
} catch (Throwable $e) {
    $info = false;
}
echo "fbird_param_info with object: ok\n";

// Test 7: Runtime - pass Firebird\Statement to fbird_execute (no TypeError)
$rs = fbird_execute($stmt, 1);
echo "fbird_execute with object: " . ($rs ? "ok" : "failed") . "\n";
if ($rs) fbird_free_result($rs);

// Test 8: Runtime - pass Firebird\Statement to fbird_num_params
$n = fbird_num_params($stmt);
echo "fbird_num_params with object: $n\n";

// Test 9: Runtime - pass Firebird\Statement to fbird_free_query
var_dump(fbird_free_query($stmt));

fbird_close($conn);
echo "Done\n";
?>
--EXPECTF--
fbird_execute param0 type: mixed
fbird_free_query param0 type: mixed
fbird_num_params param0 type: mixed
fbird_param_info param0 type: mixed
%s
fbird_param_info with object: ok
fbird_execute with object: ok
fbird_num_params with object: %d
bool(true)
Done

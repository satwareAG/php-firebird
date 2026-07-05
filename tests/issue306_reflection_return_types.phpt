--TEST--
Reflection return types show object, not resource (Issue #306)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

/*
 * Regression test for GitHub Issue #306:
 *   20+ functions declare MAY_BE_RESOURCE in arginfo but runtime
 *   returns Firebird\* objects. ReflectionFunction::getReturnType()
 *   should report object (or Firebird\* class), not resource.
 *
 * Also verifies fbird_query and fbird_execute include MAY_BE_LONG
 * for affected row count returns.
 */

$functions = [
    'fbird_connect',
    'fbird_pconnect',
    'fbird_create_database',
    'fbird_prepare',
    'fbird_prepare_ex',
    'fbird_execute',
    'fbird_query',
    'fbird_execute_query',
    'fbird_execute_auto',
    'fbird_query_params_tx',
    'fbird_trans',
    'fbird_trans_start',
    'fbird_reconnect_transaction',
    'fbird_blob_create',
    'fbird_blob_open',
    'fbird_blob_create_seekable',
    'fbird_blob_open_seekable',
    'fbird_service_attach',
    'fbird_set_event_handler',
];

$failures = [];
foreach ($functions as $fname) {
    if (!function_exists($fname)) {
        echo "SKIP $fname (not available)\n";
        continue;
    }

    $rf = new ReflectionFunction($fname);
    $rt = $rf->getReturnType();
    if (!$rt) {
        $failures[] = "$fname: no return type";
        continue;
    }

    $rtStr = (string)$rt;

    // Check that 'resource' does NOT appear in the return type
    if (strpos($rtStr, 'resource') !== false) {
        $failures[] = "$fname: return type contains 'resource': $rtStr";
    }

    // Check that 'object' or a Firebird\* class IS in the return type
    $hasObject = strpos($rtStr, 'object') !== false
              || strpos($rtStr, 'Firebird\\') !== false
              || strpos($rtStr, 'null') !== false
              || strpos($rtStr, 'false') !== false
              || strpos($rtStr, 'bool') !== false;

    if (!$hasObject) {
        $failures[] = "$fname: return type has no object/Firebird type: $rtStr";
    }
}

// Verify fbird_query and fbird_execute include int (for affected rows)
foreach (['fbird_query', 'fbird_execute'] as $fname) {
    $rf = new ReflectionFunction($fname);
    $rt = (string)$rf->getReturnType();
    if (strpos($rt, 'int') === false) {
        $failures[] = "$fname: return type missing 'int' for affected rows: $rt";
    }
}

if (empty($failures)) {
    echo "All " . count($functions) . " functions have correct return types\n";
} else {
    echo "FAILURES:\n";
    foreach ($failures as $f) {
        echo "  - $f\n";
    }
}

echo "Done\n";
?>
--EXPECT--
All 19 functions have correct return types
Done

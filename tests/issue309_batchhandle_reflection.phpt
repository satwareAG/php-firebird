--TEST--
Firebird\BatchHandle has 6 methods visible via Reflection (Issue #309)
--EXTENSIONS--
firebird
--SKIPIF--
<?php
require_once 'skipif.inc';
if (!function_exists('fbird_batch_create')) {
    die('skip IBatch API requires Firebird 4.0+');
}
?>
--FILE--
<?php

/*
 * Regression test for GitHub Issue #309:
 *   Firebird\BatchHandle was registered with NULL methods table.
 *   ReflectionClass should show 6 public methods.
 */

$rc = new ReflectionClass('Firebird\BatchHandle');

echo "class: " . $rc->getName() . "\n";
echo "isFinal: " . var_export($rc->isFinal(), true) . "\n";

$methods = $rc->getMethods(ReflectionMethod::IS_PUBLIC);
echo "method count: " . count($methods) . "\n";

$expected = [
    'getBlobAlignment',
    'setDefaultBpb',
    'cancel',
    'execute',
    'add',
    'addBlob',
];

foreach ($expected as $name) {
    $has = $rc->hasMethod($name);
    echo "has $name: " . var_export($has, true) . "\n";
}

echo "\nDone\n";
?>
--EXPECT--
class: Firebird\BatchHandle
isFinal: true
method count: 6
has getBlobAlignment: true
has setDefaultBpb: true
has cancel: true
has execute: true
has add: true
has addBlob: true

Done

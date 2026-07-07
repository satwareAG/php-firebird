--TEST--
fbird_delete_user: arginfo requires exactly 2 parameters (HF-3 regression)
--DESCRIPTION--
Regression guard for spec-v11.0.1-hotfixes.md HF-3.

Prior to v11.0.1 the arginfo for fbird_delete_user declared 3 required
parameters (service_handle, user_name, password) while the C implementation
parses only "zs" (service + user_name). This made every valid 2-arg call
throw "ArgumentCountError: Too few arguments (2 passed and at least 3 expected)".

This test verifies via Reflection that the declared arginfo now matches
the C implementation (2 required parameters, no password).
--SKIPIF--
<?php
if (!function_exists('fbird_delete_user')) {
    die('skip fbird_delete_user not available');
}
?>
--FILE--
<?php
$rf = new ReflectionFunction('fbird_delete_user');

echo "numberOfRequiredParameters: ";
var_dump($rf->getNumberOfRequiredParameters());

echo "numberOfParameters: ";
var_dump($rf->getNumberOfParameters());

echo "parameters:\n";
foreach ($rf->getParameters() as $p) {
    printf("  #%d %s (optional=%s)\n",
        $p->getPosition(),
        $p->getName(),
        $p->isOptional() ? 'true' : 'false'
    );
}

echo "has 'password' parameter: ";
$hasPassword = false;
foreach ($rf->getParameters() as $p) {
    if ($p->getName() === 'password') {
        $hasPassword = true;
        break;
    }
}
var_dump($hasPassword);
?>
--EXPECT--
numberOfRequiredParameters: int(2)
numberOfParameters: int(2)
parameters:
  #0 service_handle (optional=false)
  #1 user_name (optional=false)
has 'password' parameter: bool(false)

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

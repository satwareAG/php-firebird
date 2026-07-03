--TEST--
Firebird\Event: method registration smoke test (Phase H, OC-2)
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
?>
--FILE--
<?php
echo "=== Firebird\\Event method existence ===\n";

$expected = ['wait', 'cancel', 'getName', 'getCount'];
foreach ($expected as $method) {
    printf("%-10s: %s\n", $method, method_exists('Firebird\\Event', $method) ? 'OK' : 'MISSING');
}

echo "\n=== Reflection signatures ===\n";

$rc = new ReflectionClass('Firebird\\Event');
$methods = $rc->getMethods();
printf("Method count: %d (expected 4)\n", count($methods));

foreach ($methods as $m) {
    $params = array_map(
        fn($p) => '$' . $p->getName() . ($p->isOptional() ? ' = default' : ''),
        $m->getParameters()
    );
    printf("%s(%s): %s\n", $m->getName(), implode(', ', $params), $m->getReturnType());
}

echo "\n=== Default values ===\n";
$waitMethod = $rc->getMethod('wait');
foreach ($waitMethod->getParameters() as $p) {
    printf("%s: optional=%s default=%s\n",
        $p->getName(),
        $p->isOptional() ? 'yes' : 'no',
        $p->isDefaultValueAvailable() ? var_export($p->getDefaultValue(), true) : 'N/A'
    );
}

echo "\n=== Done ===\n";
?>
--EXPECT--
=== Firebird\Event method existence ===
wait      : OK
cancel    : OK
getName   : OK
getCount  : OK

=== Reflection signatures ===
Method count: 4 (expected 4)
wait($timeout = default): bool
cancel(): bool
getName(): string
getCount(): int

=== Default values ===
timeout: optional=yes default=-1.0

=== Done ===

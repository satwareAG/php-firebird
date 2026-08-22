--TEST--
Firebird\Connection resource lifetime: owned ref (#576)
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// Scenario 1 (issue #576 core): two Connection objects on the same DSN.
// Destroying the first must not free the resource under the second.
$a = fbird_connect($test_base);
$b = fbird_connect($test_base);
var_dump($a instanceof Firebird\Connection);
var_dump($b instanceof Firebird\Connection);
unset($a); // old bug: entry freed here, $b->isConnected() read freed memory
var_dump($b->isConnected());
$r = fbird_query("SELECT 1 AS X FROM RDB\$DATABASE");
var_dump($r !== false);
if ($r !== false) { fbird_free_query($r); }

// Scenario 2: explicit close() then isConnected() === false, double close safe
$c = fbird_connect($test_base);
var_dump($c->isConnected());
$c->close();
var_dump($c->isConnected());
$c->close(); // second close is a no-op, no crash
// explicit close() closes the shared server link for ALL holders
// ($b still wraps the same DSN-cached entry from scenario 1)
var_dump($b->isConnected());

// Scenario 3 (#576 regression guard for the default-link semantics):
// an unused fbird_connect() return keeps the default link alive.
fbird_connect($test_base);
$r = fbird_query("SELECT 1 AS X FROM RDB\$DATABASE"); // no link argument
var_dump($r !== false);
if ($r !== false) { fbird_free_query($r); }

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(true)
done

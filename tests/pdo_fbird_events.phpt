--TEST--
PDO Firebird: Event API — register, wait, count, cancel
--EXTENSIONS--
firebird
--SKIPIF--
<?php
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';

$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

/* Verify constants exist */
var_dump(defined('PDO::FBIRD_ATTR_EVENT_NAMES'));
var_dump(defined('PDO::FBIRD_ATTR_EVENT_WAIT'));
var_dump(defined('PDO::FBIRD_ATTR_EVENT_CANCEL'));
var_dump(defined('PDO::FBIRD_ATTR_EVENT_COUNT'));

/* Register events */
$pdo->setAttribute(PDO::FBIRD_ATTR_EVENT_NAMES, ['test_event_1', 'test_event_2']);
echo "events registered\n";

/* Get registered event names */
$names = $pdo->getAttribute(PDO::FBIRD_ATTR_EVENT_NAMES);
var_dump($names);

/* Get event counts before any event fired (should be all zeros) */
$counts = $pdo->getAttribute(PDO::FBIRD_ATTR_EVENT_COUNT);
echo "initial counts: ";
var_dump($counts);

/* Fire an event from a second connection, then wait on the first */
$pdo2 = pdo_fbird_connect();
$pdo2->exec("EXECUTE BLOCK AS BEGIN POST_EVENT 'test_event_1'; END");

/* Now wait — the event should fire immediately since we just posted it */
$pdo->setAttribute(PDO::FBIRD_ATTR_EVENT_WAIT, true);
echo "wait completed\n";

/* Get event counts — test_event_1 should have count > 0 */
$counts = $pdo->getAttribute(PDO::FBIRD_ATTR_EVENT_COUNT);
echo "test_event_1 fired: " . ($counts['test_event_1'] > 0 ? "yes" : "no") . "\n";
echo "test_event_2 fired: " . ($counts['test_event_2'] > 0 ? "yes" : "no") . "\n";

/* Cancel events */
$pdo->setAttribute(PDO::FBIRD_ATTR_EVENT_CANCEL, true);
echo "events cancelled\n";

/* After cancel, names should be null */
$names = $pdo->getAttribute(PDO::FBIRD_ATTR_EVENT_NAMES);
var_dump($names);

$pdo2 = null;
$pdo = null;
echo "Done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
events registered
array(2) {
  [0]=>
  string(12) "test_event_1"
  [1]=>
  string(12) "test_event_2"
}
initial counts: array(2) {
  ["test_event_1"]=>
  int(0)
  ["test_event_2"]=>
  int(0)
}
wait completed
test_event_1 fired: yes
test_event_2 fired: yes
events cancelled
NULL
Done

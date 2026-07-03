--TEST--
OOP: Event methods (getName, getCount, cancel) after live event
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc';
if (!function_exists('proc_open')) die('skip requires proc_open');
?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);
if (!$conn) die("Cannot connect\n");

// Create test table and trigger
$tr = fbird_trans($conn);
fbird_query($tr, "CREATE TABLE event_methods_test (id INTEGER)");
fbird_query($tr, "CREATE TRIGGER t_methods_event AFTER INSERT ON event_methods_test
                  AS BEGIN POST_EVENT 'my_event'; END");
fbird_commit($tr);

// Register event handler
$event = fbird_set_event_handler($conn, function($name) {
    return true;
}, 'my_event');

var_dump($event instanceof \Firebird\Event);

// Before any event fires:
// getName() returns first registered event name
var_dump($event->getName());
// getCount() returns 0
var_dump($event->getCount());

// Spawn child to insert and fire the trigger
$cmd = sprintf(
    '<?php $c = fbird_connect("%s", "%s", "%s"); $t = fbird_trans($c); fbird_query($t, "INSERT INTO event_methods_test (id) VALUES (1)"); fbird_commit($t); fbird_close($c); ?>',
    addslashes($test_base),
    addslashes($user),
    addslashes($password)
);

$child = proc_open(
    ['php', '-d', 'extension=' . realpath('modules/firebird.so')],
    [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']],
    $pipes
);
fwrite($pipes[0], $cmd);
fclose($pipes[0]);

// Wait for the event
$event->wait(10.0);
proc_close($child);

// After event fired:
var_dump($event->getName());
$count = $event->getCount();
var_dump($count >= 1);

// Cancel the event handler
var_dump($event->cancel());
// getCount() unchanged after cancel
var_dump($event->getCount() === $count);

fbird_close($conn);
echo "done\n";
?>
--EXPECTF--
bool(true)
string(8) "my_event"
int(0)
string(8) "my_event"
bool(true)
bool(true)
bool(true)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

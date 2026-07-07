--TEST--
Live event testing with DB triggers (OC-3)
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
if (!function_exists('proc_open')) {
    die('skip requires proc_open');
}
?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);
if (!$conn) die("Cannot connect\n");

// Create test table and trigger
$tr = fbird_trans($conn);
fbird_query($tr, "CREATE TABLE event_live_test (id INTEGER)");
fbird_query($tr, "CREATE TRIGGER t_post_event AFTER INSERT ON event_live_test
                  AS BEGIN POST_EVENT 'test_event'; END");
fbird_commit($tr);

// Helper: spawn a child PHP process that connects and inserts
$cmd = sprintf(
    '<?php $c = fbird_connect("%s", "%s", "%s"); $t = fbird_trans($c); fbird_query($t, "INSERT INTO event_live_test (id) VALUES (1)"); fbird_commit($t); fbird_close($c); ?>',
    addslashes($test_base),
    addslashes($user),
    addslashes($password)
);

// === Part 1: Procedural fbird_poll_event ===
echo "--- Procedural: fbird_set_event_handler + fbird_poll_event ---\n";

$callbackInvoked = false;
$callbackEvent = '';

$event = fbird_set_event_handler($conn, function($name) use (&$callbackInvoked, &$callbackEvent) {
    $callbackInvoked = true;
    $callbackEvent = $name;
    return true;
}, 'test_event');

var_dump($event instanceof \Firebird\Event || is_resource($event));

// Spawn child process to insert (fires trigger on commit)
$child = proc_open(
    ['php', '-d', 'extension=' . realpath('modules/firebird.so')],
    [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']],
    $pipes
);
fwrite($pipes[0], $cmd);
fclose($pipes[0]);

// Poll for the event (10 second timeout)
$result = fbird_poll_event($event, 10000);

// Hard-kill child before close to prevent zombie holding DB lock
proc_terminate($child, 9);
proc_close($child);

var_dump($result !== false && $result !== null);
var_dump($callbackInvoked);
var_dump($callbackEvent);

fbird_free_event_handler($event);

// === Part 2: OOP Event::wait ===
echo "--- OOP: Event::wait ---\n";

$callbackInvoked2 = false;
$event2 = fbird_set_event_handler($conn, function($name) use (&$callbackInvoked2) {
    $callbackInvoked2 = true;
    return true;
}, 'test_event');

var_dump($event2 instanceof \Firebird\Event);

// Spawn another child with id=2
$cmd2 = sprintf(
    '<?php $c = fbird_connect("%s", "%s", "%s"); $t = fbird_trans($c); fbird_query($t, "INSERT INTO event_live_test (id) VALUES (2)"); fbird_commit($t); fbird_close($c); ?>',
    addslashes($test_base),
    addslashes($user),
    addslashes($password)
);

$child2 = proc_open(
    ['php', '-d', 'extension=' . realpath('modules/firebird.so')],
    [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']],
    $pipes2
);
fwrite($pipes2[0], $cmd2);
fclose($pipes2[0]);

// OOP wait with 10 second timeout
$waitResult = $event2->wait(10.0);

// Hard-kill child before close to prevent zombie holding DB lock
proc_terminate($child2, 9);
proc_close($child2);

var_dump($waitResult);
var_dump($callbackInvoked2);

// === Part 3: OOP Event methods ===
echo "--- OOP: Event::getName / getCount / cancel ---\n";

var_dump(is_string($event2->getName()));
$count = $event2->getCount();
var_dump(is_int($count));

$cancelResult = $event2->cancel();
var_dump($cancelResult);
var_dump($event2->getCount() === $count);

fbird_close($conn);
echo "done\n";
?>
--EXPECTF--
--- Procedural: fbird_set_event_handler + fbird_poll_event ---
bool(true)
bool(true)
bool(true)
%s
--- OOP: Event::wait ---
bool(true)
bool(true)
bool(true)
--- OOP: Event::getName / getCount / cancel ---
bool(true)
bool(true)
bool(true)
bool(true)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

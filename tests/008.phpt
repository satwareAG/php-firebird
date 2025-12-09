--TEST--
InterBase: event handling
--SKIPIF--
<?php
if (PHP_OS == "WINNT") echo "skip";
if (PHP_DEBUG) echo "skip: Disabled in debug build until memory leak is fixed (See GitHub issue 45)";
if (true) die("skip: Event handling is unstable and thread-unsafe in current architecture (see Issue #46)");
include("skipif.inc");
?>
--FILE--
<?php

require("firebird.inc");

$count = 0;

function event_callback($event_name, $link = null)
{
	global $count;
    // echo "Triggered: $event_name\n";
	if ($event_name == "TEST1") echo "FAIL TEST1\n";
	return (++$count < 5); /* cancel event */
}

$conn = ibase_connect($test_base, $user, $password);
if (!$conn) die("Connection failed");

ibase_query($conn, "CREATE PROCEDURE pevent AS BEGIN POST_EVENT 'TEST1'; POST_EVENT 'TEST2'; END");
ibase_commit($conn);

// Register handler for TEST1 then free it - it should NOT fire
$ev = ibase_set_event_handler($conn, 'event_callback', 'TEST1');
ibase_free_event_handler($ev);

// Register handler for TEST2 - it SHOULD fire 5 times then cancel
ibase_set_event_handler($conn, 'event_callback', 'TEST2');

usleep(500000);

for ($i = 0; $i < 8; $i++) {
	ibase_query($conn, "EXECUTE PROCEDURE pevent");
	ibase_commit($conn);

	usleep(300000);
}

usleep(500000);

if (!$count || $count > 5) echo "FAIL ($count)\n";
echo "end of test\n";

ibase_close($conn);
?>
--EXPECT--
end of test

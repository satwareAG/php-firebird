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

require("interbase.inc");

 = 0;

function event_callback()
{
	global ;
	if ( == "TEST1") echo "FAIL TEST1\n";
	return (++ < 5); /* cancel event */
}

 = ibase_connect();

ibase_query("CREATE PROCEDURE pevent AS BEGIN POST_EVENT 'TEST1'; POST_EVENT 'TEST2'; END");
ibase_commit();

 = ibase_set_event_handler('event_callback','TEST1');
ibase_free_event_handler();

ibase_set_event_handler('event_callback','TEST2');

usleep(5E+5);

for ( = 0;  < 8; ++) {
	ibase_query("EXECUTE PROCEDURE pevent");
	ibase_commit();

	usleep(3E+5);
}

usleep(5E+5);

if (! ||  > 5) echo "FAIL ()\n";
echo "end of test\n";

?>
--EXPECT--
end of test

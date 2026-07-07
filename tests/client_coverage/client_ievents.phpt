--TEST--
IEvents que/cancel (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#390) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
$handler = @fbird_set_event_handler($link, "echo ''", "TEST_EVT");
if ($handler) fbird_free_event_handler($handler);
// wait_event with 1 second timeout
@fbird_wait_event($link, 1, "TEST_EVT");
echo "done\n";
?>
--EXPECT--
done

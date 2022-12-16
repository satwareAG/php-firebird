--TEST--
Bug #46247 (ibase_set_event_handler() is allowing to pass callback without event)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("interbase.inc");

$db = ibase_connect($test_base);

function test() { }

try {
    ibase_set_event_handler();
} catch (ArgumentCountError $e) {
    echo $e->getMessage() . "\n";
}

try {
    ibase_set_event_handler('test', 1);
} catch(TypeError) {
    echo $e->getMessage() . "\n";
}

ibase_set_event_handler($db, 'test', 1);

try {
    ibase_set_event_handler(NULL, 'test', 1);
} catch(TypeError) {
    echo $e->getMessage() . "\n";
}

try {
    ibase_set_event_handler('foo', 1);
 } catch(TypeError) {
     echo $e->getMessage() . "\n";
 }
ibase_set_event_handler($db, 'foo', 1);
try {
    ibase_set_event_handler(NULL, 'foo', 1);
} catch(TypeError) {
     echo $e->getMessage() . "\n";
}
?>
--EXPECTF--
Wrong parameter count for ibase_set_event_handler()
Wrong parameter count for ibase_set_event_handler()

Warning: ibase_set_event_handler(): Callback argument foo is not a callable function in /usr/src/php-firebird/tests/bug46247.php on line 30

Warning: ibase_set_event_handler(): Callback argument foo is not a callable function in /usr/src/php-firebird/tests/bug46247.php on line 34
Wrong parameter count for ibase_set_event_handler()

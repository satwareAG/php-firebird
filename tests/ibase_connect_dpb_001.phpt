--TEST--
ibase_connect() basic attach and SELECT 1 using defaults
--SKIPIF--
<?php
if (!extension_loaded('interbase')) {
    die('skip interbase extension not available');
}
--FILE--
<?php
require __DIR__ . '/config.inc';

// Allow overriding host/path from the environment to target
// specific Docker engines (e.g. localhost/3052:/firebird/data/test.fdb).
$host = getenv('FB_HOST') ?: $host;

$link = ibase_connect($host);
var_dump($link !== false);

$res = ibase_query($link, 'SELECT 1 FROM RDB$DATABASE');
$row = ibase_fetch_row($res);
ibase_free_result($res);
ibase_close($link);

var_dump((int) $row[0]);
--EXPECTF--
bool(true)
int(1)

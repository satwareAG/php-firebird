--TEST--
pdo_fbird: PDO::quote() doubles single quotes and wraps in quotes
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

// Basic string
echo $pdo->quote("hello") . "\n";

// String with single quote
echo $pdo->quote("it's") . "\n";

// String with multiple quotes
echo $pdo->quote("it''s a 'test'") . "\n";

// Empty string
echo $pdo->quote("") . "\n";

// String with special chars (no NULL bytes)
echo $pdo->quote("line1\nline2") . "\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
'hello'
'it''s'
'it''''s a ''test'''
''
'line1
line2'
Done

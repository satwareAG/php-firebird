--TEST--
Issue #14: Transaction::begin() must not require manual include of fbird_trans_begin wrapper
--SKIPIF--
<?php
include("skipif.inc");
// Skip if Firebird\Transaction is already registered as a C internal class
// (Phase B implementation registers it natively, making the PHP wrapper redundant)
if (class_exists('Firebird\\Transaction', false)) die('skip Firebird\\Transaction already registered as internal class');
?>
--FILE--
<?php

require __DIR__ . "/firebird.inc";
require_once __DIR__ . "/../src/Firebird/Database.php";
require_once __DIR__ . "/../src/Firebird/Transaction.php";

use Firebird\Database;
use Firebird\Transaction;

$connResource = fbird_connect($test_base);
$db = Database::fromResource($connResource, $test_base);

$tx = Transaction::begin($db);

var_dump($tx instanceof Transaction);

$tx->commit();
$db->close();

echo "OK\n";
?>
--EXPECT--
bool(true)
OK

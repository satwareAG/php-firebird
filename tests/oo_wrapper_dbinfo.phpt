--TEST--
Firebird\DbInfo userland wrapper: fromConnection, getters, toArray, fromArray
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';
require_once __DIR__ . '/../vendor/autoload.php';

use Firebird\Database;
use Firebird\DbInfo;

$conn = fbird_connect($test_base, $user, $password);
$db = Database::fromResource($conn, $test_base);

// fromConnection()
$info = DbInfo::fromConnection($db->getResource());
var_dump($info instanceof DbInfo);

// getOdsVersionString()
$ods = $info->getOdsVersionString();
var_dump(is_string($ods));
var_dump($ods !== '');

// formatted memory helpers
var_dump(is_string($info->getCurrentMemoryFormatted()));
var_dump(is_string($info->getMaxMemoryFormatted()));
var_dump(is_string($info->getPageSizeFormatted()));

// version checks return bool
var_dump(is_bool($info->isFirebird3OrLater()));
var_dump(is_bool($info->isFirebird4OrLater()));
var_dump(is_bool($info->isFirebird5OrLater()));

// IO stats
var_dump(is_int($info->getTotalIoOperations()));
var_dump(is_float($info->getReadWriteRatio()) || is_int($info->getReadWriteRatio()));

// toArray()
$arr = $info->toArray();
var_dump(is_array($arr));
var_dump(count($arr) > 0);

// fromArray() round-trip
$info2 = DbInfo::fromArray($arr);
var_dump($info2 instanceof DbInfo);
var_dump($info2->getOdsVersionString() === $info->getOdsVersionString());

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done

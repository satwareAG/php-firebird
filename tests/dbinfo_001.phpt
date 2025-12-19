--TEST--
DbInfo: Rich database information structure
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require_once __DIR__ . '/../src/Firebird/DbInfo.php';

use Firebird\DbInfo;

// Test 1: Create from array
echo "Test 1: Create from array\n";
$info = DbInfo::fromArray([
    'reads' => 1000,
    'writes' => 500,
    'fetches' => 2000,
    'marks' => 100,
    'current_memory' => 16777216, // 16 MB
    'max_memory' => 67108864, // 64 MB
    'page_size' => 16384, // 16 KB
    'num_buffers' => 2048,
    'allocation' => 10000,
    'attachment_id' => 42,
    'ods_version' => 13,
    'ods_minor_version' => 1,
    'sweep_interval' => 20000,
    'no_reserve' => false,
    'forced_writes' => true,
    'isc_version' => 'WI-V4.0.0.2496 Firebird 4.0',
    'firebird_version' => 'WI-V4.0.0.2496 Firebird 4.0',
    'connection_id' => 123,
    'transaction_count' => 5,
    'db_name' => '/var/lib/firebird/test.fdb',
]);

var_dump($info->reads);
var_dump($info->writes);
var_dump($info->pageSize);
var_dump($info->attachmentId);

// Test 2: ODS version string
echo "\nTest 2: ODS version string\n";
var_dump($info->getOdsVersionString());

// Test 3: Formatted memory
echo "\nTest 3: Formatted memory\n";
var_dump($info->getCurrentMemoryFormatted());
var_dump($info->getMaxMemoryFormatted());
var_dump($info->getPageSizeFormatted());

// Test 4: Version checks
echo "\nTest 4: Version checks\n";
var_dump($info->isFirebird3OrLater());
var_dump($info->isFirebird4OrLater());
var_dump($info->isFirebird5OrLater());

// Test 5: I/O statistics
echo "\nTest 5: I/O statistics\n";
var_dump($info->getTotalIoOperations());
var_dump($info->getReadWriteRatio());

// Test 6: toArray round-trip
echo "\nTest 6: toArray round-trip\n";
$array = $info->toArray();
var_dump(isset($array['reads']));
var_dump($array['reads'] === 1000);
var_dump($array['ods_version'] === 13);

// Test 7: Empty/default values
echo "\nTest 7: Empty/default values\n";
$emptyInfo = DbInfo::fromArray([]);
var_dump($emptyInfo->reads);
var_dump($emptyInfo->pageSize);
var_dump($emptyInfo->firebirdVersion);
var_dump($emptyInfo->isFirebird3OrLater());

// Test 8: Debug info
echo "\nTest 8: Debug info\n";
$debug = $info->__debugInfo();
var_dump(isset($debug['odsVersion']));
var_dump(isset($debug['ioStats']));

// Test 9: Zero writes ratio
echo "\nTest 9: Zero writes ratio\n";
$infoNoWrites = DbInfo::fromArray(['reads' => 100, 'writes' => 0]);
var_dump($infoNoWrites->getReadWriteRatio());

echo "\nDone.\n";
?>
--EXPECT--
Test 1: Create from array
int(1000)
int(500)
int(16384)
int(42)

Test 2: ODS version string
string(4) "13.1"

Test 3: Formatted memory
string(5) "16 MB"
string(5) "64 MB"
string(5) "16 KB"

Test 4: Version checks
bool(true)
bool(true)
bool(true)

Test 5: I/O statistics
int(1500)
float(2)

Test 6: toArray round-trip
bool(true)
bool(true)
bool(true)

Test 7: Empty/default values
int(0)
int(0)
string(0) ""
bool(false)

Test 8: Debug info
bool(true)
bool(true)

Test 9: Zero writes ratio
float(0)

Done.

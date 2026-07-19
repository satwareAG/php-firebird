--TEST--
BlobId: Type-safe BLOB identifier value object
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php

require("firebird.inc");
require_once __DIR__ . '/../src/Firebird/BlobId.php';

use Firebird\BlobId;

$db = fbird_connect($test_base);

// Test 1: Create from valid string (colon format - standard from extension)
echo "Test 1: Create from valid string\n";
$blobId = BlobId::fromString('00000001:0002');
var_dump($blobId->toString());
var_dump($blobId->getHigh());
var_dump($blobId->getLow());

// Test 2: __toString() for backward compatibility
echo "\nTest 2: __toString()\n";
var_dump((string)$blobId);
var_dump("$blobId");

// Test 3: NULL BLOB
echo "\nTest 3: NULL BLOB\n";
$nullBlob = BlobId::null();
var_dump($nullBlob->isNull());
var_dump($nullBlob->isNotNull());
var_dump($nullBlob->toString());

// Test 4: Non-NULL check
echo "\nTest 4: Non-NULL check\n";
var_dump($blobId->isNull());
var_dump($blobId->isNotNull());

// Test 5: Equality
echo "\nTest 5: Equality\n";
$blobId2 = BlobId::fromString('00000001:0002');
$blobId3 = BlobId::fromString('00000001:0003');
var_dump($blobId->equals($blobId2));
var_dump($blobId->equals($blobId3));
var_dump($blobId->equals('00000001:0002'));
var_dump($blobId->equals('00000001:0003'));

// Test 6: fromParts
echo "\nTest 6: fromParts\n";
$blobId4 = BlobId::fromParts(1, 2);
var_dump($blobId4->toString());
var_dump($blobId4->equals($blobId));

// Test 7: Invalid format
echo "\nTest 7: Invalid format\n";
try {
    BlobId::fromString('invalid');
    echo "ERROR: Should have thrown\n";
} catch (InvalidArgumentException $e) {
    echo "Caught: " . substr($e->getMessage(), 0, 25) . "...\n";
}

// Test 8: tryFromString
echo "\nTest 8: tryFromString\n";
$valid = BlobId::tryFromString('0x0000000100000002');
$invalid = BlobId::tryFromString('invalid');
var_dump($valid !== null);
var_dump($invalid === null);

// Test 9: isValidFormat static method
echo "\nTest 9: isValidFormat\n";
var_dump(BlobId::isValidFormat('00000001:0002'));
var_dump(BlobId::isValidFormat('ABCDEF01:2345'));
var_dump(BlobId::isValidFormat('0xABCDEF0123456789')); // Hex format also valid
var_dump(BlobId::isValidFormat('invalid'));
var_dump(BlobId::isValidFormat('0x12345'));

// Test 10: Case insensitivity and format conversion
echo "\nTest 10: Case insensitivity and format conversion\n";
$upper = BlobId::fromString('ABCDEF01:2345');
$lower = BlobId::fromString('abcdef01:2345');
$hex = BlobId::fromString('0xabcdef0123456789'); // Hex format converted to colon
var_dump($upper->equals($lower));
var_dump($upper->toString()); // Should be lowercase colon format
var_dump($hex->toString()); // Should be converted to colon format

// Test 11: Use with actual BLOB
echo "\nTest 11: Integration with fbird_blob functions\n";
$trans = fbird_trans($db);
$blob = fbird_blob_create($trans);
fbird_blob_add($blob, "Test BLOB content");
$blobIdStr = fbird_blob_close($blob);

// Create BlobId from returned string
$blobIdObj = BlobId::fromString($blobIdStr);
var_dump($blobIdObj->isNotNull());

// Use BlobId with fbird_blob_open (string conversion)
$stream = fbird_blob_open($trans, (string)$blobIdObj);
if ($stream) {
    $content = fbird_blob_get($stream, 100);
    var_dump($content);
    fbird_blob_close($stream);
}

fbird_commit($trans);

// Test 12: Debug info
echo "\nTest 12: Debug info\n";
$debug = $blobId->__debugInfo();
var_dump(isset($debug['id']));
var_dump(isset($debug['high']));
var_dump(isset($debug['low']));
var_dump(isset($debug['isNull']));

fbird_close($db);

echo "\nDone.\n";
?>
--EXPECTF--
Test 1: Create from valid string
string(17) "00000001:00000002"
int(1)
int(2)

Test 2: __toString()
string(17) "00000001:00000002"
string(17) "00000001:00000002"

Test 3: NULL BLOB
bool(true)
bool(false)
string(17) "00000000:00000000"

Test 4: Non-NULL check
bool(false)
bool(true)

Test 5: Equality
bool(true)
bool(false)
bool(true)
bool(false)

Test 6: fromParts
string(17) "00000001:00000002"
bool(true)

Test 7: Invalid format
Caught: Invalid BLOB ID format%s...

Test 8: tryFromString
bool(true)
bool(true)

Test 9: isValidFormat
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)

Test 10: Case insensitivity and format conversion
bool(true)
string(17) "abcdef01:00002345"
string(17) "abcdef01:00002345"

Test 11: Integration with fbird_blob functions
bool(true)
string(17) "Test BLOB content"

Test 12: Debug info
bool(true)
bool(true)
bool(true)
bool(true)

Done.

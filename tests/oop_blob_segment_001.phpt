--TEST--
OOP: Blob segmented write and read round-trip
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
$tx = oop_begin_transaction($conn);

// Create blob and write in segments
$blob = \Firebird\Blob::create($conn, $tx);
var_dump($blob instanceof \Firebird\Blob);

$segment1 = str_repeat('A', 1000);
$segment2 = str_repeat('B', 2000);
$segment3 = str_repeat('C', 500);

$blob->write($segment1);
$blob->write($segment2);
$blob->write($segment3);
$blob->close();

$blobId = $blob->getId();
var_dump(is_string($blobId));
var_dump(strlen($blobId) > 0);

// Open and read in segments
$blob2 = \Firebird\Blob::open($conn, $tx, $blobId);
var_dump($blob2 instanceof \Firebird\Blob);

// Read in small chunks
$allData = '';
while (($chunk = $blob2->read(512)) !== false && $chunk !== '') {
    $allData .= $chunk;
}
$blob2->close();

// Verify data integrity
$expected = $segment1 . $segment2 . $segment3;
var_dump(strlen($allData) === strlen($expected));
var_dump($allData === $expected);

$tx->commit();
oop_close($conn);
echo "done\n";
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

--TEST--
OOP: Blob create/write/read round-trip via Firebird\Blob
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
$tx = oop_begin_transaction($conn);

// Create a blob and write data
$blob = \Firebird\Blob::create($conn, $tx);
var_dump($blob instanceof \Firebird\Blob);

$data = str_repeat('Hello Firebird Blob! ', 10);
$blob->write($data);
$blob->close();

// Open the blob for reading
$blobId = $blob->getId();
var_dump(is_string($blobId));

$blob2 = \Firebird\Blob::open($conn, $tx, $blobId);
var_dump($blob2 instanceof \Firebird\Blob);

$read = $blob2->read(strlen($data));
var_dump($read === $data);
$blob2->close();

$tx->commit();
oop_close($conn);
echo "done\n";
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

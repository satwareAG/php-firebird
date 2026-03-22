--TEST--
Firebird\Blob: class registration, write, read, info
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

var_dump(class_exists('Firebird\\Blob'));

$conn = new Firebird\Connection($test_base, $user, $password);
$tr = $conn->beginTransaction();

// Create a blob, write data, close it
$blob = Firebird\Blob::create($conn, $tr);
var_dump($blob instanceof Firebird\Blob);

$data = str_repeat('Hello Firebird Blob! ', 10);
$blob->write($data);
$blob->close();

// Open the blob for reading
$blob2 = Firebird\Blob::open($conn, $tr, $blob->getId());
var_dump($blob2 instanceof Firebird\Blob);

$read = $blob2->read(strlen($data));
var_dump($read === $data);
$blob2->close();

$tr->commit();
$conn->close();
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
done

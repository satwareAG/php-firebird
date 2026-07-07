--TEST--
OOP: Negative tests - call methods on closed connection
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();
var_dump($conn->isConnected());

// Close the connection
oop_close($conn);
var_dump($conn->isConnected());

// beginTransaction on closed connection should throw
try {
    $conn->beginTransaction();
    echo "FAIL: no exception thrown\n";
} catch (\Firebird\ConnectionException $e) {
    echo "caught ConnectionException: " . $e->getMessage() . "\n";
} catch (\Throwable $e) {
    echo "caught " . get_class($e) . ": " . $e->getMessage() . "\n";
}

// prepare on closed connection should throw
try {
    $conn->prepare('SELECT 1 FROM RDB$DATABASE', oop_begin_transaction($conn));
    echo "FAIL: no exception thrown\n";
} catch (\Firebird\ConnectionException $e) {
    echo "caught ConnectionException: " . $e->getMessage() . "\n";
} catch (\Throwable $e) {
    echo "caught " . get_class($e) . ": " . $e->getMessage() . "\n";
}

echo "done\n";
?>
--EXPECTF--
bool(true)
bool(false)
caught ConnectionException: Not connected
caught ConnectionException: Not connected
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

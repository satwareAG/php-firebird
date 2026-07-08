--TEST--
PDO Definition: PDOException fields
--CREDITS--
v12.1.0 M1-21 (#348)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';
echo "=== PDOException fields ===\n";
$pdo = pdo_fbird_connect();

try {
    $pdo->exec("DELETE FROM nonexistent_exception_fields");
    echo "FAIL should have thrown\n";
} catch (\PDOException $e) {
    echo "OK instanceof PDOException: " . ($e instanceof \PDOException ? 'yes' : 'no') . "\n";
    echo "OK instanceof RuntimeException: " . ($e instanceof \RuntimeException ? 'yes' : 'no') . "\n";
    echo "OK getCode: " . $e->getCode() . "\n";
    echo "OK errorInfo present: " . (is_array($e->errorInfo) ? 'yes' : 'no') . "\n";
    echo "OK errorInfo[0]: " . $e->errorInfo[0] . "\n";
    echo "OK getMessage has content: " . (strlen($e->getMessage()) > 0 ? 'yes' : 'no') . "\n";
    echo "OK getFile: " . (strlen($e->getFile()) > 0 ? 'yes' : 'no') . "\n";
    echo "OK getLine > 0: " . ($e->getLine() > 0 ? 'yes' : 'no') . "\n";
}

echo "=== DONE ===\n";
?>
--EXPECTF--
=== PDOException fields ===
OK instanceof PDOException: yes
OK instanceof RuntimeException: yes
OK getCode: %s
OK errorInfo present: yes
OK errorInfo[0]: %s
OK getMessage has content: yes
OK getFile: yes
OK getLine > 0: yes
=== DONE ===

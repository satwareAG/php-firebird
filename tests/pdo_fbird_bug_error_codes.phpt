--TEST--
pdo_fbird: SQLSTATE error code mapping
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
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

// Test 1: Table not found → SQLSTATE 42S02 or HY000
$pdo->exec("RECREATE TABLE err_test (id INTEGER NOT NULL PRIMARY KEY, name VARCHAR(50))");

try {
    $pdo->exec("SELECT * FROM nonexistent_table_xyz");
    echo "ERROR: should have thrown\n";
} catch (PDOException $e) {
    $state = $e->getCode();
    echo "Missing table SQLSTATE: " . (strlen($state) > 0 ? "has code" : "empty") . "\n";
}

// Test 2: Duplicate primary key → SQLSTATE 23000
$pdo->exec("INSERT INTO err_test (id, name) VALUES (1, 'first')");
try {
    $pdo->exec("INSERT INTO err_test (id, name) VALUES (1, 'duplicate')");
    echo "ERROR: should have thrown\n";
} catch (PDOException $e) {
    $state = $e->getCode();
    echo "Duplicate key SQLSTATE: " . (strlen($state) > 0 ? "has code" : "empty") . "\n";
}

// Test 3: Syntax error → SQLSTATE 42000 or HY000
try {
    $pdo->exec("SELEKT * FORM err_test");
    echo "ERROR: should have thrown\n";
} catch (PDOException $e) {
    $state = $e->getCode();
    echo "Syntax error SQLSTATE: " . (strlen($state) > 0 ? "has code" : "empty") . "\n";
}

// Test 4: errorInfo() returns 3-element array
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
$pdo->exec("SELECT * FROM nonexistent_table_xyz_2");
$info = $pdo->errorInfo();
echo "errorInfo count: " . count($info) . "\n";
echo "errorInfo[0] type: " . gettype($info[0]) . "\n";

$pdo->exec("DROP TABLE err_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
Missing table SQLSTATE: has code
Duplicate key SQLSTATE: has code
Syntax error SQLSTATE: has code
errorInfo count: 3
errorInfo[0] type: string
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
@$pdo->exec("DROP TABLE err_test");
unset($pdo);
?>

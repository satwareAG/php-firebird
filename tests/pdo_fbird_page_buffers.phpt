--TEST--
PDO_FBIRD: page_buffers DSN option and getAttribute round-trip (#240)
--SKIPIF--
<?php
if (!extension_loaded('PDO')) die('skip PDO not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/config.inc';
$dbEnv = getenv('FIREBIRD_DB_PATH') ?: getenv('FIREBIRD_DATABASE');
$db = ($dbEnv ?: '/firebird/data/test.fdb');
$dsn = "fbird:host={$host};dbname={$db}";
try { new PDO($dsn, $user, $password); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/config.inc';
$dbEnv = getenv('FIREBIRD_DB_PATH') ?: getenv('FIREBIRD_DATABASE');
$db = ($dbEnv ?: '/firebird/data/test.fdb');
$dsn_base = "fbird:host={$host};dbname={$db}";

// Connect WITH page_buffers=2048 in DSN
$pdo = new PDO($dsn_base . ';page_buffers=2048', $user, $password,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);

// getAttribute should return the value parsed from DSN
$buffers = $pdo->getAttribute(PDO::FBIRD_ATTR_PAGE_BUFFERS);
echo "page_buffers from getAttribute: ";
var_dump($buffers);

// Verify the value matches
if ($buffers === 2048) {
    echo "PASS: value matches\n";
} else {
    echo "FAIL: expected 2048, got $buffers\n";
}

// setAttribute after connect should fail with IM001
try {
    $pdo->setAttribute(PDO::FBIRD_ATTR_PAGE_BUFFERS, 4096);
    echo "FAIL: setAttribute should have thrown\n";
} catch (PDOException $e) {
    if (strpos($e->getCode(), 'IM001') !== false) {
        echo "PASS: setAttribute correctly rejected with IM001\n";
    } else {
        echo "FAIL: expected IM001, got " . $e->getCode() . "\n";
    }
}

// Connect WITHOUT page_buffers — should default to 0
$pdo2 = new PDO($dsn_base, $user, $password,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
$buffers2 = $pdo2->getAttribute(PDO::FBIRD_ATTR_PAGE_BUFFERS);
echo "default page_buffers: ";
var_dump($buffers2);

echo "Done\n";
?>
--EXPECT--
page_buffers from getAttribute: int(2048)
PASS: value matches
PASS: setAttribute correctly rejected with IM001
default page_buffers: int(0)
Done

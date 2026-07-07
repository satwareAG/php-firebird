--TEST--
PDO Definition: optional fetch_err (errorCode + errorInfo)
--CREDITS--
v12.1.0 M1-6 (#333)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Optional: fetch_err (errorCode + errorInfo) ===\n";

// Use ERRMODE_SILENT so we can inspect error state
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);

// 1. Clean state - no error
$clean = $pdo->errorCode();
echo "OK errorCode (clean): " . var_export($clean, true) . "\n";

// 2. Bad table - should return a non-zero SQLSTATE
@$pdo->exec("DELETE FROM nonexistent_table_err");
$code = $pdo->errorCode();
$info = $pdo->errorInfo();
echo "OK errorCode (bad table): $code\n";
echo "OK errorInfo[0] (SQLSTATE): " . $info[0] . "\n";
echo "OK errorInfo has message: " . (strlen($info[2] ?? '') > 0 ? 'yes' : 'no') . "\n";

// 3. Constraint violation on duplicate PK
$pdo->exec("RECREATE TABLE test_err (id INT PRIMARY KEY)");
$pdo->exec("INSERT INTO test_err VALUES (1)");
@$pdo->exec("INSERT INTO test_err VALUES (1)");
$code = $pdo->errorCode();
echo "OK errorCode (dup PK): $code\n";

// 4. Successful query returns to clean state
$pdo->exec("DELETE FROM test_err WHERE id = 1");
$code = $pdo->errorCode();
echo "OK errorCode (after success): " . var_export($code, true) . "\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect(); @$pdo->exec("DROP TABLE test_err"); ?>
--EXPECTF--
=== Optional: fetch_err (errorCode + errorInfo) ===
OK errorCode (clean): NULL
OK errorCode (bad table): %s
OK errorInfo[0] (SQLSTATE): %s
OK errorInfo has message: yes
OK errorCode (dup PK): %s
OK errorCode (after success): '00000'
=== DONE ===

--TEST--
PDO Definition: optional quoter (PDO::quote())
--CREDITS--
v12.1.0 M1-4 (#331)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/../../pdo_fbird.inc';

echo "=== Optional: quoter ===\n";
$pdo = pdo_fbird_connect();

// Basic string quoting
$q = $pdo->quote("hello");
echo "OK quote(hello): $q\n";

// Quote doubling
$q = $pdo->quote("O'Brien");
echo "OK quote(O'Brien): $q\n";

// Empty string
$q = $pdo->quote("");
echo "OK quote(empty): $q\n";

// Integer (gets string-quoted)
$q = $pdo->quote(42);
echo "OK quote(42): $q\n";

// NULL - may deprecate in PHP 8.4+; skip if it throws
try {
    $q = @$pdo->quote(null);
    echo "OK quote(null): " . var_export($q, true) . "\n";
} catch (\Throwable $e) {
    echo "OK quote(null): deprecated/unsupported\n";
}

// Verify quoted value round-trips through query
$pdo->exec("RECREATE TABLE test_quoter (val VARCHAR(100))");
$quoted = $pdo->quote("test; drop table--");
$pdo->exec("INSERT INTO test_quoter VALUES ($quoted)");
$result = $pdo->query("SELECT val FROM test_quoter")->fetchColumn();
echo "OK round-trip: $result\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/../../pdo_fbird.inc'; @$pdo = pdo_fbird_connect(); @$pdo->exec("DROP TABLE test_quoter"); ?>
--EXPECTF--
=== Optional: quoter ===
OK quote(hello): 'hello'
OK quote(O'Brien): 'O''Brien'
OK quote(empty): ''
OK quote(42): '42'
OK quote(null): %s
OK round-trip: test; drop table--
=== DONE ===

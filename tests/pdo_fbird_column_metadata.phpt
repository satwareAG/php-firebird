--TEST--
pdo_fbird: column metadata via describe and fetch types
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

$pdo->exec("RECREATE TABLE colmeta_test (
    id INTEGER NOT NULL PRIMARY KEY,
    name VARCHAR(100),
    price DOUBLE PRECISION,
    active BOOLEAN,
    created TIMESTAMP DEFAULT CURRENT_TIMESTAMP
)");
$pdo->exec("INSERT INTO colmeta_test (id, name, price, active) VALUES (1, 'Widget', 9.99, TRUE)");

$stmt = $pdo->query("SELECT id, name, price, active, created FROM colmeta_test");

/* Verify column count */
echo "columns: " . $stmt->columnCount() . "\n";

/* Fetch and verify types */
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "id: " . $row['ID'] . "\n";
echo "id_type: " . gettype($row['ID']) . "\n";
echo "name: " . $row['NAME'] . "\n";
echo "name_type: " . gettype($row['NAME']) . "\n";
echo "price_type: " . gettype($row['PRICE']) . "\n";
echo "active_type: " . gettype($row['ACTIVE']) . "\n";
echo "created_type: " . gettype($row['CREATED']) . "\n";

/* Verify column names via NUM fetch */
$stmt2 = $pdo->query("SELECT id AS my_id, name AS my_name FROM colmeta_test");
$row2 = $stmt2->fetch(PDO::FETCH_ASSOC);
echo "alias: " . implode(',', array_keys($row2)) . "\n";

$pdo->exec("DROP TABLE colmeta_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
columns: 5
id: 1
id_type: integer
name: Widget
name_type: string
price_type: double
active_type: boolean
created_type: string
alias: MY_ID,MY_NAME
Done

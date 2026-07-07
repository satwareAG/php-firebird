--TEST--
pdo_fbird: PDO::exec() with multiple semicolon-separated DML statements (batch DML)
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/../pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';
$pdo = pdo_fbird_connect();

/* Setup: create a clean test table */
$pdo->exec("RECREATE TABLE pdo_batch_dml_t (id INTEGER, val VARCHAR(50))");

/* --- Test 1: single statement (no trailing semicolon) still works --- */
$n = $pdo->exec("INSERT INTO pdo_batch_dml_t VALUES (1, 'a')");
echo "single insert: $n\n";

/* --- Test 2: single statement with trailing semicolon --- */
$n = $pdo->exec("INSERT INTO pdo_batch_dml_t VALUES (2, 'b');");
echo "single insert trailing semi: $n\n";

/* --- Test 3: two INSERT statements separated by semicolon --- */
$n = $pdo->exec("INSERT INTO pdo_batch_dml_t VALUES (3, 'c'); INSERT INTO pdo_batch_dml_t VALUES (4, 'd')");
echo "two inserts: $n\n";

/* --- Test 4: three statements, affected rows summed --- */
$n = $pdo->exec(
    "INSERT INTO pdo_batch_dml_t VALUES (5, 'e');" .
    "INSERT INTO pdo_batch_dml_t VALUES (6, 'f');" .
    "INSERT INTO pdo_batch_dml_t VALUES (7, 'g')"
);
echo "three inserts: $n\n";

/* --- Test 5: UPDATE across multiple rows --- */
$n = $pdo->exec(
    "UPDATE pdo_batch_dml_t SET val = 'x' WHERE id <= 3;" .
    "UPDATE pdo_batch_dml_t SET val = 'y' WHERE id >= 6"
);
echo "two updates: $n\n";

/* --- Test 6: semicolon inside a string literal is NOT a separator --- */
$n = $pdo->exec("UPDATE pdo_batch_dml_t SET val = 'hello;world' WHERE id = 1");
echo "literal semicolon: $n\n";

/* Verify row count */
$stmt = $pdo->query("SELECT COUNT(*) FROM pdo_batch_dml_t");
$cnt = $stmt->fetchColumn();
echo "total rows: $cnt\n";

/* --- Test 7: error in second statement rolls back and returns false --- */
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
$result = $pdo->exec(
    "INSERT INTO pdo_batch_dml_t VALUES (99, 'ok');" .
    "INSERT INTO nonexistent_table_xyz VALUES (1, 2, 3)"
);
echo "error batch result: " . ($result === false ? "false" : $result) . "\n";

/* Verify that the first insert was rolled back (row 99 must not exist) */
$stmt = $pdo->query("SELECT COUNT(*) FROM pdo_batch_dml_t WHERE id = 99");
$cnt99 = $stmt->fetchColumn();
echo "row 99 after rollback: $cnt99\n";

/* Cleanup */
$pdo->exec("DROP TABLE pdo_batch_dml_t");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
single insert: 1
single insert trailing semi: 1
two inserts: 2
three inserts: 3
two updates: 5
literal semicolon: 1
total rows: 7
error batch result: false
row 99 after rollback: 0
Done

--CLEAN--
<?php
require_once __DIR__ . '/../pdo_fbird.inc';
$pdo = pdo_fbird_connect();
@$pdo->exec('DROP TABLE pdo_batch_dml_t');
unset($pdo);
?>

--TEST--
pdo_fbird: DDL — sequences, views, stored procedures
--SKIPIF--
<?php
if (!extension_loaded('firebird')) die('skip firebird not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/pdo_fbird.inc';
try { $pdo = pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
/* ALTER SEQUENCE RESTART WITH semantics changed in Firebird 4.0:
   FB3: RESTART WITH n → next value = n+1; FB4+: RESTART WITH n → next value = n
   Note: PDO::ATTR_SERVER_VERSION returns the *client library* version, not the server.
   Use ENGINE_VERSION system context to detect the actual server version. */
$v = $pdo->query("SELECT rdb\$get_context('SYSTEM', 'ENGINE_VERSION') FROM RDB\$DATABASE")->fetchColumn();
if (floatval($v) < 4.0) die('skip requires Firebird 4.0+ (ALTER SEQUENCE RESTART semantics)');
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

/* Sequence (generator) */
try { $pdo->exec("DROP SEQUENCE ddl2_seq"); } catch (Throwable $e) {}
$pdo->exec("CREATE SEQUENCE ddl2_seq");
$pdo->exec("ALTER SEQUENCE ddl2_seq RESTART WITH 100");
$stmt = $pdo->query("SELECT NEXT VALUE FOR ddl2_seq FROM RDB\$DATABASE");
$val = $stmt->fetchColumn();
echo "seq: $val\n";
$pdo->exec("DROP SEQUENCE ddl2_seq");
echo "seq dropped\n";

/* View — DDL needs hard commit before DML can see new table metadata.
 * In autocommit mode, Firebird uses commit_retaining which preserves
 * the transaction snapshot; beginTransaction() forces a hard commit. */
$pdo->exec("RECREATE TABLE ddl2_base (id INTEGER, name VARCHAR(30))");
$pdo->beginTransaction();
$pdo->exec("INSERT INTO ddl2_base VALUES (1, 'test')");
$pdo->commit();
try { $pdo->exec("DROP VIEW ddl2_view"); } catch (Throwable $e) {}
$pdo->exec("CREATE VIEW ddl2_view AS SELECT id, name FROM ddl2_base WHERE id > 0");
$stmt = $pdo->query("SELECT * FROM ddl2_view");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "view: {$row['ID']}-{$row['NAME']}\n";
$pdo->exec("DROP VIEW ddl2_view");
$pdo->exec("DROP TABLE ddl2_base");
echo "view dropped\n";

/* Stored procedure */
try { $pdo->exec("DROP PROCEDURE ddl2_proc"); } catch (Throwable $e) {}
$pdo->exec("CREATE PROCEDURE ddl2_proc (x INTEGER) RETURNS (y INTEGER) AS BEGIN y = x * 2; SUSPEND; END");
$stmt = $pdo->query("SELECT y FROM ddl2_proc(21)");
$val = $stmt->fetchColumn();
echo "proc: $val\n";
$pdo->exec("DROP PROCEDURE ddl2_proc");
echo "proc dropped\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
seq: 100
seq dropped
view: 1-test
view dropped
proc: 42
proc dropped
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP VIEW ddl2_view");
@$pdo->exec("DROP PROCEDURE ddl2_proc");
@$pdo->exec("DROP TABLE ddl2_base");
@$pdo->exec("DROP GENERATOR ddl2_seq");
unset($pdo);
?>

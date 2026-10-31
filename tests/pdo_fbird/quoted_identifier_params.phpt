--TEST--
SEC FR-005 (#734): named-param preprocessor must track double-quoted identifier state - :name inside delimited identifiers is never substituted ("" escapes preserved)
--CREDITS--
v13.4.0 security hardening train (#728) - FR-005 wire observer
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
$pdo->exec('RECREATE TABLE pdo_qid_t (plain_col INTEGER, "my:col" INTEGER)');

/* 1. insert with named params + a delimited identifier containing a colon:
 *    RED anchor - on unpatched code the preprocessor substitutes :col inside
 *    "my:col", corrupting the statement */
$stmt = $pdo->prepare('INSERT INTO pdo_qid_t (plain_col, "my:col") VALUES (:a, :b)');
if (!$stmt instanceof PDOStatement) {
    $err = $pdo->errorInfo();
    echo 'insert with quoted identifier: FAIL: prepare returned false (' . substr((string) $err[2], 0, 70) . ")\n";
} elseif (!$stmt->execute(['a' => 5, 'b' => 6])) {
    $err = $stmt->errorInfo();
    echo 'insert with quoted identifier: FAIL: execute failed (' . substr((string) $err[2], 0, 70) . ")\n";
} else {
    echo 'insert with quoted identifier: ok' . "\n";
}

/* 2. select through the quoted identifier: value must round-trip */
$res = $pdo->query('SELECT "my:col" FROM pdo_qid_t');
$v = $res instanceof PDOStatement ? $res->fetchColumn() : false;
echo 'select quoted identifier: ' . ($v == 6 ? 'ok' : 'FAIL: ' . var_export($v, true)) . "\n";

/* 3. guard: real named params outside quotes still substitute */
$stmt = $pdo->prepare('SELECT plain_col FROM pdo_qid_t WHERE plain_col = :plain');
$stmt->execute(['plain' => 5]);
echo 'real named param: ' . ($stmt->fetchColumn() == 5 ? 'ok' : 'FAIL') . "\n";

/* 4. escaped quotes inside the identifier ("") survive preprocessing */
$pdo->exec('RECREATE TABLE pdo_qid2_t ("we""ird:col" INTEGER)');
$stmt = $pdo->prepare('SELECT "we""ird:col" FROM pdo_qid2_t');
echo 'escaped-quote identifier: ' . ($stmt instanceof PDOStatement && $stmt->execute() ? 'ok' : 'FAIL: prepare/execute failed') . "\n";

$pdo->exec('DROP TABLE pdo_qid_t');
$pdo->exec('DROP TABLE pdo_qid2_t');
echo "=== DONE ===\n";
?>
--EXPECT--
insert with quoted identifier: ok
select quoted identifier: ok
real named param: ok
escaped-quote identifier: ok
=== DONE ===

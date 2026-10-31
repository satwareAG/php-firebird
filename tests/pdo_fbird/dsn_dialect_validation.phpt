--TEST--
SEC FR-005 (#734): PDO fbird DSN dialect must be validated against {1,2,3} - garbage rejected with PDOException; wire proof that the DSN dialect reaches the engine
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
global $dsn, $user, $password;
$base = preg_replace('/dialect=\d+;?/', '', $dsn); /* strip if present */

/* 1. garbage dialect must throw a PDOException with the validation message */
try {
    new PDO($base . ';dialect=99', $user, $password, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
    echo "dialect=99: FAIL - connected with garbage dialect\n";
} catch (PDOException $e) {
    echo 'dialect=99 rejected: ' . (strpos($e->getMessage(), 'Invalid dialect') !== false ? 'ok' : 'FAIL: ' . substr($e->getMessage(), 0, 70)) . "\n";
}

/* 2. wire proof the DSN dialect reaches the engine: in dialect 1 the
 *    double-quoted "HELLO" is a string literal; in dialect 3 it is an
 *    identifier (unknown column). If both behave per-dialect, the DSN
 *    dialect landed in the attach DPB. */
$pdo1 = new PDO($base . ';dialect=1', $user, $password, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
$res1 = $pdo1->query('SELECT "HELLO" FROM RDB$DATABASE');
$v1 = $res1 instanceof PDOStatement ? $res1->fetchColumn() : false;
/* dialect 1: the double-quoted token is a STRING LITERAL -> row value HELLO */
echo 'dialect=1 quoted-string literal: ' . ($v1 === 'HELLO' ? 'ok' : 'FAIL: ' . var_export($v1, true)) . "\n";

/* dialect 3: the double-quoted token is an IDENTIFIER -> statement error.
 * This driver's query() returns false instead of throwing even under
 * ERRMODE_EXCEPTION, and engine wording differs across versions (FB3
 * "Column unknown", FB5 reworded) - so assert the semantics: statement
 * fails in dialect 3. */
$pdo3 = new PDO($base . ';dialect=3', $user, $password, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
$res3 = $pdo3->query('SELECT "HELLO" FROM RDB$DATABASE');
echo 'dialect=3 quoted-identifier: ' . ($res3 === false ? 'ok' : 'FAIL: dialect-3 treated the identifier as a literal') . "\n";

/* 3. absent dialect defaults to 3 and works */
$def = new PDO(preg_replace('/;dialect=\d+/', '', $dsn), $user, $password, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
echo 'default dialect connect: ' . ($def->query('SELECT 1 FROM RDB$DATABASE')->fetchColumn() == 1 ? 'ok' : 'FAIL') . "\n";

echo "=== DONE ===\n";
?>
--EXPECT--
dialect=99 rejected: ok
dialect=1 quoted-string literal: ok
dialect=3 quoted-identifier: ok
default dialect connect: ok
=== DONE ===

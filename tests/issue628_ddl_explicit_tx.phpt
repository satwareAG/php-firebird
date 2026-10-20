--TEST--
Issue #628: DDL on an explicit transaction is transactional and engine-faithful (ADR-003)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

/* Locked-in behavior (ADR-003, #628 resolution):
 * 1. Same-tx DML against uncommitted DDL fails LOUDLY with the engine's
 *    -204 "Table unknown" - never silently.
 * 2. fbird_commit persists the DDL (table exists, 0 rows).
 * 3. fbird_rollback drops the DDL (table gone).
 * The engine (FB3 + FB5, isql SET AUTODDL OFF + raw client cross-check 2026-10-20)
 * does not compile DML against a relation created in the same uncommitted tx. */

$c = fbird_connect($test_base);
var_dump($c instanceof Firebird\Connection);

$t = fbird_trans_start($c, [
    'access_mode' => FBIRD_WRITE,
    'isolation' => FBIRD_COMMITTED | FBIRD_REC_VERSION,
    'lock_resolution' => FBIRD_WAIT,
]);
var_dump($t instanceof Firebird\Transaction);

$qD = fbird_prepare_ex($c, 'CREATE TABLE T628_STDDL (id INTEGER)', $t);
var_dump($qD !== false);
var_dump(fbird_execute($qD) === true);
fbird_free_query($qD);

// 1. same-tx DML fails loudly with -204 (precise error, not silent)
$qI = @fbird_prepare_ex($c, 'INSERT INTO T628_STDDL VALUES (1)', $t);
var_dump($qI !== false);
echo "errmsg-has-table-unknown: ";
var_dump(strpos(fbird_errmsg(), 'Table unknown') !== false);

// 2. commit persists the DDL
var_dump(fbird_commit($t));
$qC = fbird_query($c, 'SELECT COUNT(*) FROM T628_STDDL');
var_dump($qC !== false);
$r = fbird_fetch_row($qC);
echo "rows-after-commit: " . (int)$r[0] . "\n";
fbird_free_result($qC);

// post-commit DML on a fresh transaction works
$t2 = fbird_trans_start($c, ['access_mode' => FBIRD_WRITE, 'isolation' => FBIRD_COMMITTED]);
$qI2 = fbird_prepare_ex($c, 'INSERT INTO T628_STDDL VALUES (2)', $t2);
var_dump($qI2 !== false);
var_dump(fbird_execute($qI2) !== false);
fbird_free_query($qI2);
var_dump(fbird_commit($t2));

// 3. rollback drops uncommitted DDL
$t3 = fbird_trans_start($c, ['access_mode' => FBIRD_WRITE, 'isolation' => FBIRD_COMMITTED]);
$qD3 = fbird_prepare_ex($c, 'CREATE TABLE T628_ROLL (id INTEGER)', $t3);
var_dump(fbird_execute($qD3) === true);
fbird_free_query($qD3);
var_dump(fbird_rollback($t3));
$qG = @fbird_prepare_ex($c, 'SELECT id FROM T628_ROLL', null);
var_dump($qG !== false);
echo "rolled-back-ddl-gone: ";
var_dump($qG === false && strpos(fbird_errmsg(), 'Table unknown') !== false);
if ($qG) fbird_free_query($qG);

fbird_close($c);
echo "done\n";
?>
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
errmsg-has-table-unknown: bool(true)
bool(true)
bool(true)
rows-after-commit: 0
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
rolled-back-ddl-gone: bool(true)
done

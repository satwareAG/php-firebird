--TEST--
fbird_query_params_tx: explicit link + transaction + parameterized SQL
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

// ---- Setup -----------------------------------------------------------------
$dbh = fbird_connect($test_base);
if (!$dbh) die("FAIL: connect: " . fbird_errmsg() . "\n");

fbird_query($dbh, "RECREATE TABLE qptx_test (ITEST INTEGER, CTEST VARCHAR(30))");
fbird_commit($dbh); // close implicit DDL transaction before starting explicit one

$trans = fbird_trans(FBIRD_WRITE | FBIRD_COMMITTED | FBIRD_WAIT, $dbh);
if (!$trans) die("FAIL: trans: " . fbird_errmsg() . "\n");

// ---- Test 1: function exists -----------------------------------------------
var_dump(function_exists('fbird_query_params_tx'));

// ---- Test 2: DML without params (INSERT) -----------------------------------
$r1 = fbird_query_params_tx(
    $dbh, $trans,
    "INSERT INTO qptx_test (ITEST, CTEST) VALUES (9901, 'no_params')"
);
var_dump(is_int($r1) || $r1 === true);

// ---- Test 3: DML with params (INSERT + placeholders) -----------------------
$r2 = fbird_query_params_tx(
    $dbh, $trans,
    "INSERT INTO qptx_test (ITEST, CTEST) VALUES (?, ?)",
    [9902, 'with_params']
);
var_dump(is_int($r2) || $r2 === true);

// ---- Test 4: SELECT returns a resource -------------------------------------
$rs = fbird_query_params_tx(
    $dbh, $trans,
    "SELECT ITEST, CTEST FROM qptx_test WHERE ITEST = ?",
    [9901]
);
var_dump(is_resource($rs));

$row = fbird_fetch_assoc($rs);
var_dump($row !== false);
var_dump((int)$row['ITEST'] === 9901);
var_dump(trim($row['CTEST']) === 'no_params');
fbird_free_result($rs);

// ---- Test 5: SELECT with no rows returns empty result ----------------------
$rs2 = fbird_query_params_tx(
    $dbh, $trans,
    "SELECT ITEST FROM qptx_test WHERE ITEST = ?",
    [99999]
);
var_dump(is_resource($rs2));
var_dump(fbird_fetch_assoc($rs2) === false);
fbird_free_result($rs2);

// ---- Cleanup ---------------------------------------------------------------
fbird_rollback($trans);
fbird_close($dbh);
echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
--CLEAN--
<?php
require __DIR__ . '/../firebird.inc';
$dbh = fbird_connect($test_base);
if ($dbh) {
    @fbird_query($dbh, "DROP TABLE qptx_test");
    fbird_commit($dbh);
    fbird_close($dbh);
}
?>

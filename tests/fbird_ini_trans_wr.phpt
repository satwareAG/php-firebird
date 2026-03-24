--TEST--
fbird.default_trans_params INI setting (WRITE)
--SKIPIF--
<?php include("skipif.inc"); ?>
--INI--
fbird.default_trans_params=8
--FILE--
<?php
require_once("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);
$table = "T_INI_WR_" . time();

fbird_query($conn, "CREATE TABLE $table (id INTEGER)");
fbird_commit($conn);

$tr = fbird_trans($conn);
echo "Started transaction with default_trans_params=8 (WRITE)\n";

$res = fbird_query($tr, "INSERT INTO $table (id) VALUES (1)");

if ($res !== false) {
    echo "Insert succeeded in WRITE transaction\n";
} else {
    echo "Insert failed unexpectedly: " . fbird_errmsg() . "\n";
}

fbird_commit($tr);
fbird_close($conn);
?>
--EXPECT--
Started transaction with default_trans_params=8 (WRITE)
Insert succeeded in WRITE transaction

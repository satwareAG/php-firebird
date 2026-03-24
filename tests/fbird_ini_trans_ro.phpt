--TEST--
fbird.default_trans_params INI setting (READ-ONLY)
--SKIPIF--
<?php include("skipif.inc"); ?>
--INI--
fbird.default_trans_params=6
--FILE--
<?php
require_once("firebird.inc");

$table = "T_INI_RO_" . time();

// Setup table using a separate process to avoid the restrictive INI
$setup_code = <<<PHP
<?php
require_once("firebird.inc");
\$conn = fbird_connect(\$test_base, \$user, \$password);
fbird_query(\$conn, "CREATE TABLE $table (id INTEGER)");
fbird_commit(\$conn);
fbird_close(\$conn);
PHP;

$tmpfile = tempnam(sys_get_temp_dir(), 'fbs');
file_put_contents($tmpfile, $setup_code);
exec(PHP_BINARY . " -n -d extension=modules/firebird.so $tmpfile");
unlink($tmpfile);

// Now test restrictive transaction in THIS process
$conn = fbird_connect($test_base, $user, $password);
$tr = fbird_trans($conn);
echo "Started transaction with default_trans_params=6 (READ | COMMITTED)\n";

$res = @fbird_query($tr, "INSERT INTO $table (id) VALUES (1)");

if ($res === false) {
    echo "Insert failed as expected in READ-ONLY transaction\n";
} else {
    echo "Insert succeeded unexpectedly\n";
}

fbird_rollback($tr);
fbird_close($conn);
?>
--EXPECT--
Started transaction with default_trans_params=6 (READ | COMMITTED)
Insert failed as expected in READ-ONLY transaction

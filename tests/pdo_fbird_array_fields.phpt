--TEST--
PDO Firebird: Array field support (read INTEGER and VARCHAR arrays)
--EXTENSIONS--
pdo
--SKIPIF--
<?php
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
if (!extension_loaded('firebird')) die('skip firebird ext needed for array insert');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';

$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$db   = getenv('FIREBIRD_DB_PATH') ?: getenv('FIREBIRD_DATABASE') ?: '/firebird/data/test.fdb';
$user = getenv('FIREBIRD_USER') ?: 'SYSDBA';
$pass = getenv('FIREBIRD_PASSWORD') ?: 'masterkey';
$fbird_dsn = $host . ':' . $db;

/* Step 1: Create table and insert array data via fbird API (only way to write arrays) */
$c = fbird_connect($fbird_dsn, $user, $pass);
fbird_query($c, "RECREATE TABLE pdo_arr_test (id INTEGER NOT NULL, int_arr INTEGER[5], str_arr VARCHAR(20)[3])");
fbird_commit($c);

/* Row 1: NULL arrays */
fbird_query($c, "INSERT INTO pdo_arr_test (id) VALUES (1)");
fbird_commit($c);

/* Row 2: integer array */
$ia = array(1=>10, 2=>20, 3=>30, 4=>40, 5=>50);
fbird_query($c, "INSERT INTO pdo_arr_test (id, int_arr) VALUES (?, ?)", 2, $ia);
fbird_commit($c);

/* Row 3: varchar array */
$sa = array(1=>"hello", 2=>"world", 3=>"test");
fbird_query($c, "INSERT INTO pdo_arr_test (id, str_arr) VALUES (?, ?)", 3, $sa);
fbird_commit($c);

/* Row 4: both arrays */
fbird_query($c, "INSERT INTO pdo_arr_test (id, int_arr, str_arr) VALUES (?, ?, ?)", 4, $ia, $sa);
fbird_commit($c);

fbird_close($c);
echo "insert ok\n";

/* Step 2: Read arrays via PDO */
$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

/* NULL arrays */
$row = $pdo->query("SELECT int_arr, str_arr FROM pdo_arr_test WHERE id = 1")->fetch(PDO::FETCH_ASSOC);
var_dump($row['INT_ARR'] === null);
var_dump($row['STR_ARR'] === null);
echo "null ok\n";

/* Integer array */
$row = $pdo->query("SELECT int_arr FROM pdo_arr_test WHERE id = 2")->fetch(PDO::FETCH_NUM);
$a = $row[0];
echo "int type: " . gettype($a) . "\n";
echo "int count: " . count($a) . "\n";
echo "int[1]: " . $a[1] . "\n";
echo "int[5]: " . $a[5] . "\n";

/* Varchar array */
$row = $pdo->query("SELECT str_arr FROM pdo_arr_test WHERE id = 3")->fetch(PDO::FETCH_NUM);
$a = $row[0];
echo "str type: " . gettype($a) . "\n";
echo "str count: " . count($a) . "\n";
echo "str[1]: " . $a[1] . "\n";
echo "str[3]: " . $a[3] . "\n";

/* Both arrays */
$row = $pdo->query("SELECT int_arr, str_arr FROM pdo_arr_test WHERE id = 4")->fetch(PDO::FETCH_ASSOC);
echo "both int[3]: " . $row['INT_ARR'][3] . "\n";
echo "both str[2]: " . $row['STR_ARR'][2] . "\n";

/* Multi-row */
$rows = $pdo->query("SELECT id, int_arr FROM pdo_arr_test ORDER BY id")->fetchAll(PDO::FETCH_ASSOC);
echo "rows: " . count($rows) . "\n";
echo "row1 null: " . var_export($rows[0]['INT_ARR'] === null, true) . "\n";
echo "row2 arr: " . var_export(is_array($rows[1]['INT_ARR']), true) . "\n";

$pdo->exec("DROP TABLE pdo_arr_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
insert ok
bool(true)
bool(true)
null ok
int type: array
int count: 5
int[1]: 10
int[5]: 50
str type: array
str count: 3
str[1]: hello
str[3]: test
both int[3]: 30
both str[2]: world
rows: 4
row1 null: true
row2 arr: true
Done

--CLEAN--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect([PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]);
@$pdo->exec("DROP TABLE pdo_arr_test");
unset($pdo);
?>

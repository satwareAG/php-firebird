--TEST--
fbird_execute() statement reuse with new parameters (#128)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$db = fbird_connect($test_base);

$tr = fbird_trans($db);
fbird_query($tr, "RECREATE TABLE reuse_test (id INTEGER, name VARCHAR(50))");
fbird_commit($tr);

/* Prepare once, execute multiple times */
$tr = fbird_trans($db);
$stmt = fbird_prepare($tr, "INSERT INTO reuse_test (id, name) VALUES (?, ?)");

$r1 = fbird_execute($stmt, 1, "Alice");
var_dump($r1 !== false); /* true */

$r2 = fbird_execute($stmt, 2, "Bob");
var_dump($r2 !== false); /* true */

$r3 = fbird_execute($stmt, 3, "Charlie");
var_dump($r3 !== false); /* true */

fbird_commit($tr);

/* Verify all rows */
$tr = fbird_trans($db);
$rs = fbird_query($tr, "SELECT id, name FROM reuse_test ORDER BY id");
while ($row = fbird_fetch_assoc($rs)) {
    echo $row["ID"] . ": " . $row["NAME"] . "\n";
}
fbird_free_result($rs);
fbird_query($tr, "DROP TABLE reuse_test");
@fbird_commit($tr);
fbird_close($db);
echo "Done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
1: Alice
2: Bob
3: Charlie
Done

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE reuse_test");
    @fbird_close($db);
}
?>

--TEST--
fbird_commit_ret() resource lifecycle: statements survive, cursors survive (#131)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
$db = fbird_connect($test_base);

$tr = fbird_trans($db);
fbird_query($tr, "RECREATE TABLE cret_test (id INTEGER, name VARCHAR(50))");
fbird_commit($tr);

/* Prepare statement, execute, commit_ret, reuse */
$tr = fbird_trans($db);
$stmt = fbird_prepare($tr, "INSERT INTO cret_test (id, name) VALUES (?, ?)");

fbird_execute($stmt, 1, "Alice");
fbird_commit_ret($tr);
echo "commit_ret 1 ok\n";

/* Statement survives commit_ret — reuse it */
fbird_execute($stmt, 2, "Bob");
fbird_commit_ret($tr);
echo "commit_ret 2 ok\n";

/* Cursor after commit_ret: cursors survive in Firebird with retain */
$rs = fbird_query($tr, "SELECT id, name FROM cret_test ORDER BY id");
$row = fbird_fetch_assoc($rs);
echo "row: " . $row["ID"] . ": " . $row["NAME"] . "\n";
fbird_commit_ret($tr);

/* Cursor survives commit_ret (retain keeps cursors open) */
$row2 = fbird_fetch_assoc($rs);
echo "row2: " . $row2["ID"] . ": " . $row2["NAME"] . "\n";

/* EOF */
$row3 = fbird_fetch_assoc($rs);
var_dump($row3); /* false */
fbird_free_result($rs);

/* Final commit */
fbird_commit($tr);

/* Verify all data persisted */
$tr = fbird_trans($db);
$rs = fbird_query($tr, "SELECT id, name FROM cret_test ORDER BY id");
while ($row = fbird_fetch_assoc($rs)) {
    echo $row["ID"] . ": " . $row["NAME"] . "\n";
}
fbird_free_result($rs);
fbird_query($tr, "DROP TABLE cret_test");
@fbird_commit($tr);
fbird_close($db);
echo "Done\n";
?>
--EXPECT--
commit_ret 1 ok
commit_ret 2 ok
row: 1: Alice
row2: 2: Bob
bool(false)
1: Alice
2: Bob
Done

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

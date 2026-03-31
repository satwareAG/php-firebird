--TEST--
fbird default charset INI setting
--SKIPIF--
<?php include("skipif.inc"); ?>
--INI--
fbird.default_charset=UTF8
--FILE--
<?php
require_once("firebird.inc");

echo "Connecting with default_charset=UTF8...\n";
$conn = fbird_connect($test_base, $user, $password);

// In Firebird, there isn't a direct "get current connection charset" function in the procedural API,
// but we can verify it by inserting and selecting UTF8 data.
fbird_query($conn, "RECREATE TABLE test_charset (val VARCHAR(100) CHARACTER SET UTF8)");
fbird_commit($conn);

$utf8_str = "Héllo €";
fbird_query($conn, "INSERT INTO test_charset (val) VALUES ('$utf8_str')");
fbird_commit($conn);

$res = fbird_query($conn, "SELECT val FROM test_charset");
$row = fbird_fetch_assoc($res);
if ($row['VAL'] === $utf8_str) {
    echo "UTF8 data roundtrip successful\n";
} else {
    echo "UTF8 data roundtrip failed\n";
    var_dump($row['VAL']);
}

fbird_close($conn);
?>
--EXPECT--
Connecting with default_charset=UTF8...
UTF8 data roundtrip successful

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE test_charset");
    @fbird_close($db);
}
?>

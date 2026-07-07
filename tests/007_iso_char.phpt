--TEST--
Firebird: CHAR(10)[10] array test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
fbird_connect($test_base);

fbird_query("recreate table test_char_arr (id integer, v_char char(10)[10])");
fbird_commit();

$v_char = array();
for ($i = 1; $i <= 10; ++$i) {
    $v_char[$i] = "test" . $i;
}

echo "Inserting CHAR(10)[10]...\n";
$result = fbird_query("insert into test_char_arr (id, v_char) values (?, ?)", 1, $v_char);
if ($result === false) {
    echo "INSERT failed: " . fbird_errmsg() . "\n";
} else {
    fbird_commit();
    echo "INSERT OK\n";

    $sel = fbird_query("select * from test_char_arr");
    if ($row = fbird_fetch_assoc($sel, FBIRD_FETCH_ARRAYS)) {
        echo "Row ID: " . $row['ID'] . "\n";
        echo "V_CHAR[1]: '" . $row['V_CHAR'][1] . "'\n";
    }
    fbird_free_result($sel);
}

fbird_close();
echo "Done\n";
?>
--EXPECT--
Inserting CHAR(10)[10]...
INSERT OK
Row ID: 1
V_CHAR[1]: 'test1'
Done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

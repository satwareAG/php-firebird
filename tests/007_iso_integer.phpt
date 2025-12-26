--TEST--
Firebird: INTEGER[10] array test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
fbird_connect($test_base);

fbird_query("recreate table test_int_arr (id integer, v_int integer[10])");
fbird_commit();

$v_int = array();
for ($i = 1; $i <= 10; ++$i) {
    $v_int[$i] = $i * 100;
}

echo "Inserting INTEGER[10]...\n";
$result = fbird_query("insert into test_int_arr (id, v_int) values (?, ?)", 1, $v_int);
if ($result === false) {
    echo "INSERT failed: " . fbird_errmsg() . "\n";
} else {
    fbird_commit();
    echo "INSERT OK\n";

    $sel = fbird_query("select * from test_int_arr");
    if ($row = fbird_fetch_assoc($sel, FBIRD_FETCH_ARRAYS)) {
        echo "Row ID: " . $row['ID'] . "\n";
        echo "V_INT[1]: " . $row['V_INT'][1] . "\n";
        echo "V_INT[10]: " . $row['V_INT'][10] . "\n";
    }
    fbird_free_result($sel);
}

fbird_close();
echo "Done\n";
?>
--EXPECT--
Inserting INTEGER[10]...
INSERT OK
Row ID: 1
V_INT[1]: 100
V_INT[10]: 1000
Done

--TEST--
Firebird: VARCHAR(10)[10] small array test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
fbird_connect($test_base);

fbird_query("recreate table test_varchar10 (id integer, v_varchar varchar(10)[10])");
fbird_commit();

echo "el_size should be 10+2=12\n";
echo "ar_size should be 12*10=120\n";

$v_varchar = array();
for ($i = 1; $i <= 10; ++$i) {
    $v_varchar[$i] = "test" . $i;
}

echo "Inserting...\n";
$result = fbird_query("insert into test_varchar10 (id, v_varchar) values (?, ?)", 1, $v_varchar);
if ($result === false) {
    echo "INSERT failed: " . fbird_errmsg() . "\n";
} else {
    fbird_commit();
    echo "INSERT OK\n";

    $sel = fbird_query("select * from test_varchar10");
    if ($row = fbird_fetch_assoc($sel, FBIRD_FETCH_ARRAYS)) {
        echo "Row ID: " . $row['ID'] . "\n";
        echo "V_VARCHAR[1]: " . $row['V_VARCHAR'][1] . "\n";
        echo "V_VARCHAR[10]: " . $row['V_VARCHAR'][10] . "\n";
    }
    fbird_free_result($sel);
}

fbird_close();
echo "Done\n";
?>
--EXPECT--
el_size should be 10+2=12
ar_size should be 12*10=120
Inserting...
INSERT OK
Row ID: 1
V_VARCHAR[1]: test1
V_VARCHAR[10]: test10
Done

--TEST--
Firebird: VARCHAR[1000][10] array test
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");
fbird_connect($test_base);

fbird_query("recreate table test_varchar1000 (id integer, v_varchar varchar(1000)[10])");
fbird_commit();

echo "Preparing data...\n";
$v_varchar = array();
for ($i = 1; $i <= 10; ++$i) {
    $v_varchar[$i] = str_repeat('x', 100);  // 100 char strings
}

echo "Inserting...\n";
$result = fbird_query("insert into test_varchar1000 (id, v_varchar) values (?, ?)", 1, $v_varchar);
if ($result === false) {
    echo "INSERT failed: " . fbird_errmsg() . "\n";
} else {
    fbird_commit();
    echo "INSERT OK\n";

    echo "Selecting...\n";
    $sel = fbird_query("select * from test_varchar1000");
    if ($row = fbird_fetch_assoc($sel, FBIRD_FETCH_ARRAYS)) {
        echo "Row ID: " . $row['ID'] . "\n";
        echo "V_VARCHAR count: " . count($row['V_VARCHAR']) . "\n";
        echo "V_VARCHAR[1] length: " . strlen($row['V_VARCHAR'][1]) . "\n";
    } else {
        echo "SELECT failed\n";
    }
    fbird_free_result($sel);
}

fbird_close();
echo "Done\n";
?>
--EXPECT--
Preparing data...
Inserting...
INSERT OK
Selecting...
Row ID: 1
V_VARCHAR count: 10
V_VARCHAR[1] length: 100
Done

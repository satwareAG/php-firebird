<?php
require_once('tests/interbase.inc');

echo "Creating connections...\n";
$con = ibase_connect($test_base);
echo "con: resource id=" . get_resource_id($con) . "\n";

$pcon1 = ibase_pconnect($test_base);
echo "pcon1: resource id=" . get_resource_id($pcon1) . "\n";

$pcon2 = ibase_pconnect($test_base);
echo "pcon2: resource id=" . get_resource_id($pcon2) . "\n";

echo "Before closing - pcon1 and pcon2 same? " . (($pcon1 === $pcon2) ? "YES" : "NO") . "\n";

echo "Closing con...\n";
var_dump(ibase_close($con));

echo "Closing pcon1...\n";
var_dump(ibase_close($pcon1));

echo "pcon2 still valid? " . (is_resource($pcon2) ? "YES" : "NO") . "\n";

echo "Testing explicit query on pcon2...\n";
try {
    $res = ibase_query($pcon2, "select * from test1");
    echo "Query with explicit connection works!\n";
    ibase_free_result($res);
} catch (Exception $e) {
    echo "Query with explicit connection failed: " . $e->getMessage() . "\n";
}

echo "Testing implicit query (no connection)...\n";
try {
    $res = ibase_query("select * from test1");
    echo "Query without explicit connection works!\n";
    ibase_free_result($res);
} catch (Exception $e) {
    echo "Query without explicit connection failed: " . $e->getMessage() . "\n";
}
?>

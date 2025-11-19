<?php
require_once('tests/interbase.inc');

echo "Starting test...\n";

// First test: simple connect and query
ibase_connect($test_base);
out_table("test1");
ibase_close();

// Second test: multiple connections with closes
$con = ibase_connect($test_base);
$pcon1 = ibase_pconnect($test_base);
$pcon2 = ibase_pconnect($test_base);

echo "Closing connections...\n";
ibase_close($con);
unset($con);
ibase_close($pcon1);
unset($pcon1);

echo "Testing default link after closes...\n";
out_table("test1");

ibase_close($pcon2);
unset($pcon2);

echo "Test completed.\n";
?>

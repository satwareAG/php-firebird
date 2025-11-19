<?php
require_once('tests/interbase.inc');
require_once('tests/common.inc');

echo "Testing timezone preservation...\n";

try {
    ibase_connect($test_base);
    
    echo "Creating table with timezone fields...\n";
    ibase_query("CREATE TABLE TTEST (
        ID INTEGER,
        T1 TIME WITH TIME ZONE DEFAULT '15:45:59 Europe/Riga',
        T2 TIMESTAMP WITH TIME ZONE DEFAULT '2025-11-06 15:45:59 Europe/Riga'
    )");
    ibase_commit();
    
    echo "Inserting test data...\n";
    ibase_query("INSERT INTO TTEST (ID) VALUES (1)");
    
    echo "Querying without IBASE_UNIXTIME flag:\n";
    $q = ibase_query("SELECT * FROM TTEST");
    $row = ibase_fetch_assoc($q);
    var_dump($row);
    ibase_free_result($q);
    
    echo "Querying with IBASE_UNIXTIME flag:\n";
    $q = ibase_query("SELECT * FROM TTEST");
    $row = ibase_fetch_assoc($q, IBASE_UNIXTIME);
    var_dump($row);
    ibase_free_result($q);
    
} catch (Exception $e) {
    echo "Error: " . $e->getMessage() . "\n";
}
?>

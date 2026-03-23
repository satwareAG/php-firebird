--TEST--
pdo_fbird: BLOB column reading as string
--SKIPIF--
<?php
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Exception $e) { die('skip cannot connect'); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

pdo_fbird_with_table($pdo, 'BLOB_TEST', 'ID INTEGER, DATA BLOB SUB_TYPE TEXT', function($pdo) {
    // Insert text blob via literal
    $pdo->exec("INSERT INTO BLOB_TEST VALUES (1, 'Hello blob content')");

    // Read it back
    $row = $pdo->query("SELECT DATA FROM BLOB_TEST WHERE ID = 1")->fetch(PDO::FETCH_NUM);
    echo "blob: " . $row[0] . "\n";

    // NULL blob
    $pdo->exec("INSERT INTO BLOB_TEST VALUES (2, NULL)");
    $row = $pdo->query("SELECT DATA FROM BLOB_TEST WHERE ID = 2")->fetch(PDO::FETCH_NUM);
    echo "null: " . var_export($row[0], true) . "\n";
});

$pdo = null;
echo "Done\n";
?>
--EXPECT--
blob: Hello blob content
null: NULL
Done

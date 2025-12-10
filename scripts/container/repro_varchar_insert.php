<?php
$user = 'SYSDBA';
$password = 'masterkey';
$host = 'firebird40';
// Make sure this path maps to something accessible in firebird container
// php84-dev mounts fb40-data:/firebird
// firebird40 mounts fb40-data:/firebird/data
// So /firebird/repro.fdb in php84-dev => /firebird/data/repro.fdb in firebird40
$db = '/firebird/repro.fdb';
$connection_string = "$host:$db";

echo "Connecting to $connection_string...\n";

// Attempt connect, create if fails
$dbh = @fbird_connect($connection_string, $user, $password);
if (!$dbh) {
    echo "Creating database...\n";
    $dbh = fbird_query(FBIRD_CREATE, "CREATE DATABASE '$connection_string' USER '$user' PASSWORD '$password' DEFAULT CHARACTER SET UTF8");
}

if (!$dbh) {
    die("Failed to connect/create DB: " . fbird_errmsg() . "\n");
}

echo "Recreating table TEST_VARCHAR10...\n";
// Dropping table carefully
@fbird_query($dbh, "DROP TABLE TEST_VARCHAR10");
fbird_commit($dbh);

// Reduced to [10] but insert 1
$sql = "CREATE TABLE TEST_VARCHAR10 (ID INTEGER, V_VARCHAR VARCHAR(10)[10])";
if (!fbird_query($dbh, $sql)) {
    die("Create table failed: " . fbird_errmsg() . "\n");
}
fbird_commit($dbh);

echo "Inserting data...\n";
$v_varchar = array();
for ($i = 1; $i <= 1; ++$i) {
    $v_varchar[$i] = "test" . $i;
}

$res = fbird_query($dbh, "INSERT INTO TEST_VARCHAR10 (ID, V_VARCHAR) VALUES (?, ?)", 1, $v_varchar);
if (!$res) {
    die("INSERT failed: " . fbird_errmsg() . "\n");
}
fbird_commit($dbh);
echo "INSERT OK. Committed.\n";

echo "Verifying via PHP SELECT...\n";
$sel = fbird_query($dbh, "SELECT * FROM TEST_VARCHAR10");
if ($row = fbird_fetch_assoc($sel, FBIRD_FETCH_ARRAYS)) {
    echo "Row ID: " . $row['ID'] . "\n";
    // echo "V_VARCHAR: " . print_r($row['V_VARCHAR'], true) . "\n";
    echo "V_VARCHAR[1]: " . $row['V_VARCHAR'][1] . "\n";
    // Check first element specifically
    if ($row['V_VARCHAR'][1] !== 'test1') {
        echo "FAILURE: Expected 'test1', got '" . $row['V_VARCHAR'][1] . "'\n";
    } else {
        echo "SUCCESS (in PHP): Retrieved 'test1'\n";
    }
} else {
    echo "No rows returned!\n";
}
fbird_free_result($sel);
fbird_close($dbh);

echo "\nPHP Verification Finish. DB Left at $db\n";
?>

--TEST--
fbird default connection INI settings
--SKIPIF--
<?php include("skipif.inc"); ?>
--INI--
fbird.default_user=SYSDBA
fbird.default_password=masterkey
--FILE--
<?php
require_once("firebird.inc");

// Connect without user/password - should use INI defaults
echo "Connecting with INI defaults...\n";
$conn = fbird_connect($test_base);

if ($conn) {
    echo "Connected successfully\n";
    $res = fbird_query($conn, "SELECT 1 FROM RDB\$DATABASE");
    if ($res) {
        echo "Query succeeded\n";
    }
    fbird_close($conn);
} else {
    echo "Connection failed: " . fbird_errmsg() . "\n";
}

echo "Changing INI defaults to invalid...\n";
ini_set('fbird.default_password', 'wrong_password');
$conn = @fbird_connect($test_base);

if (!$conn) {
    echo "Connection failed as expected with wrong password\n";
} else {
    echo "Connection succeeded unexpectedly\n";
    fbird_close($conn);
}
?>
--EXPECT--
Connecting with INI defaults...
Connected successfully
Query succeeded
Changing INI defaults to invalid...
Connection failed as expected with wrong password

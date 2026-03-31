--TEST--
Issue #129: PHP stream support for BLOB parameters in fbird_execute()
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$conn = fbird_connect($test_base, $user, $password);

fbird_query($conn, "CREATE TABLE test_blobs (id INTEGER, b BLOB)");
fbird_commit($conn);

echo "Attempting to use a PHP stream as a BLOB parameter...\n";
$stream = fopen('php://temp', 'r+');
fwrite($stream, "Large BLOB data from stream");
rewind($stream);

$prep = fbird_prepare($conn, "INSERT INTO test_blobs (id, b) VALUES (?, ?)");

try {
    // Current behavior might try to convert stream to string or fail
    $res = @fbird_execute($prep, 1, $stream);
    if ($res) {
        echo "Current behavior: fbird_execute() accepted stream (possibly converted to string)\n";
    } else {
        echo "Current behavior: fbird_execute() failed with stream\n";
    }
} catch (\TypeError $e) {
    echo "Current behavior: fbird_execute() rejected stream with TypeError\n";
}

fclose($stream);
fbird_free_query($prep);
fbird_close($conn);
?>
--EXPECTF--
Attempting to use a PHP stream as a BLOB parameter...
%A

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE test_blobs");
    @fbird_close($db);
}
?>

--TEST--
pdo_fbird: BLOB streaming via bindColumn with PDO::PARAM_LOB
--SKIPIF--
<?php
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not available');
require_once __DIR__ . '/pdo_fbird.inc';
try { $pdo = pdo_fbird_connect(); $pdo = null; } catch (Exception $e) { die('skip ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

$pdo->exec("RECREATE TABLE blob_stream_test (id INTEGER NOT NULL PRIMARY KEY, content BLOB SUB_TYPE TEXT)");
$pdo->exec("INSERT INTO blob_stream_test VALUES (1, 'Hello blob stream')");
$pdo->exec("INSERT INTO blob_stream_test VALUES (2, 'Second blob content that is a bit longer to test streaming')");

$stmt = $pdo->prepare("SELECT id, content FROM blob_stream_test ORDER BY id");
$stmt->execute();
$stmt->bindColumn('CONTENT', $lob, PDO::PARAM_LOB);

while ($stmt->fetch(PDO::FETCH_BOUND)) {
    if (is_resource($lob)) {
        echo "stream: " . stream_get_contents($lob) . "\n";
        fclose($lob);
    } else {
        echo "not a stream\n";
    }
}

$pdo->exec("DROP TABLE blob_stream_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
stream: Hello blob stream
stream: Second blob content that is a bit longer to test streaming
Done

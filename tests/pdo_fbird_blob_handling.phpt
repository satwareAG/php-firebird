--TEST--
PDO_FBIRD: BLOB insert via exec, read as string and stream
--SKIPIF--
<?php
if (!extension_loaded('PDO')) die('skip PDO not loaded');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';

$pdo = pdo_fbird_connect();

$pdo->exec("RECREATE TABLE blob_test (id INTEGER, content BLOB SUB_TYPE TEXT)");

// Insert blob via exec (literal string)
$pdo->exec("INSERT INTO blob_test (id, content) VALUES (1, 'Hello blob content')");
$pdo->exec("INSERT INTO blob_test (id, content) VALUES (2, NULL)");

// Read blob as string (default)
$stmt = $pdo->query("SELECT id, content FROM blob_test ORDER BY id");
$rows = $stmt->fetchAll(PDO::FETCH_ASSOC);
echo "row1 content: " . $rows[0]['CONTENT'] . "\n";
echo "row2 content is null: " . var_export($rows[1]['CONTENT'], true) . "\n";

// Read blob as stream via bindColumn
$stmt = $pdo->query("SELECT id, content FROM blob_test WHERE id = 1");
$stmt->bindColumn('CONTENT', $blob, PDO::PARAM_LOB);
$stmt->fetch(PDO::FETCH_BOUND);
if (is_resource($blob)) {
    $stream_content = stream_get_contents($blob);
    echo "stream content: $stream_content\n";
} else {
    echo "stream content: $blob\n";
}

// Cleanup
$pdo->exec("DROP TABLE blob_test");
$pdo = null;
echo "Done\n";
?>
--EXPECT--
row1 content: Hello blob content
row2 content is null: NULL
stream content: Hello blob content
Done

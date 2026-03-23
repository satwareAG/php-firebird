--TEST--
pdo_fbird: EXECUTE BLOCK with named params preserved inside BEGIN...END
--SKIPIF--
<?php
if (!extension_loaded('pdo')) die('skip PDO not available');
if (!in_array('fbird', PDO::getAvailableDrivers())) die('skip pdo_fbird not loaded');
require_once __DIR__ . '/pdo_fbird.inc';
try { pdo_fbird_connect(); } catch (Throwable $e) { die('skip cannot connect: ' . $e->getMessage()); }
?>
--FILE--
<?php
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();

// EXECUTE BLOCK with input params — :name params in the block header
// should be preserved, not converted to ?
$sql = "EXECUTE BLOCK (p_val INTEGER = ?) RETURNS (result INTEGER) AS
BEGIN
    result = p_val + 10;
    SUSPEND;
END";

$stmt = $pdo->prepare($sql);
$stmt->execute([1]);
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Result: " . $row['RESULT'] . "\n";

$stmt->execute([11]);
$row = $stmt->fetch(PDO::FETCH_ASSOC);
echo "Result: " . $row['RESULT'] . "\n";

$pdo = null;
echo "Done\n";
?>
--EXPECT--
Result: 11
Result: 21
Done

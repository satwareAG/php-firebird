--TEST--
OOP: FBIRD_FETCH_DATE_OBJ with OOP fetch
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/common.inc';

$conn = oop_connect();

// Create table with date/time columns
fbird_query($conn, 'CREATE TABLE OOP_DATE_TEST (ID INTEGER, DT DATE, TM TIME, TS TIMESTAMP)');
fbird_query($conn, "INSERT INTO OOP_DATE_TEST VALUES (1, '2025-12-20', '14:30:45', '2025-12-20 14:30:45')");
fbird_commit($conn);

// Fetch without DATE_OBJ (string format)
$tx2 = oop_begin_transaction($conn);
$rs = oop_query($conn, $tx2, 'SELECT DT, TM, TS FROM OOP_DATE_TEST WHERE ID = 1');
$row = $rs->fetch();
var_dump(gettype($row['DT']));
var_dump(gettype($row['TM']));
var_dump(gettype($row['TS']));
$rs->close();
$tx2->commit();

// Fetch with FBIRD_FETCH_DATE_OBJ
$tx3 = oop_begin_transaction($conn);
$rs2 = oop_query($conn, $tx3, 'SELECT DT, TM, TS FROM OOP_DATE_TEST WHERE ID = 1');
$row2 = fbird_fetch_assoc($rs2, FBIRD_FETCH_DATE_OBJ);
var_dump($row2['DT'] instanceof DateTimeImmutable);
var_dump($row2['TM'] instanceof DateTimeImmutable);
var_dump($row2['TS'] instanceof DateTimeImmutable);
var_dump($row2['DT']->format('Y-m-d'));
var_dump($row2['TM']->format('H:i:s'));
var_dump($row2['TS']->format('Y-m-d H:i:s'));
$rs2->close();
$tx3->commit();

oop_close($conn);
echo "done\n";
?>
--EXPECTF--
string(6) "string"
string(6) "string"
string(6) "string"
bool(true)
bool(true)
bool(true)
string(10) "2025-12-20"
string(8) "14:30:45"
string(19) "2025-12-20 14:30:45"
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

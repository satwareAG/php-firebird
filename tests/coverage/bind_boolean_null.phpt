--TEST--
Coverage: BOOLEAN column binding — true/false/NULL roundtrip
--EXTENSIONS--
firebird
--SKIPIF--
<?php
include __DIR__ . '/../skipif.inc';
require_once __DIR__ . '/../firebird.inc';
// BOOLEAN type requires Firebird 3.0+
if (get_fb_version() < 3.0) {
    die('skip BOOLEAN type requires Firebird 3.0+');
}
?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';

$dbh = fbird_connect($test_base);
if (!$dbh) die('skip connect failed: ' . fbird_errmsg());

fbird_query($dbh, "EXECUTE BLOCK AS BEGIN
  IF (EXISTS(SELECT 1 FROM RDB\$RELATIONS WHERE RDB\$RELATION_NAME = 'BIND_BOOL_COV')) THEN
    EXECUTE STATEMENT 'DROP TABLE BIND_BOOL_COV';
END");
fbird_commit($dbh);

fbird_query($dbh, '
    CREATE TABLE BIND_BOOL_COV (
        ID    INTEGER NOT NULL PRIMARY KEY,
        B     BOOLEAN,
        V     VARCHAR(20)
    )');
fbird_commit($dbh);

$stmt = fbird_prepare($dbh, 'INSERT INTO BIND_BOOL_COV (ID, B, V) VALUES (?, ?, ?)');

// Test 1: true
echo "Test 1: bind true\n";
$r = fbird_execute($stmt, 1, true, 'yes');
var_dump($r !== false);

// Test 2: false
echo "Test 2: bind false\n";
$r = fbird_execute($stmt, 2, false, 'no');
var_dump($r !== false);

// Test 3: NULL
echo "Test 3: bind NULL to BOOLEAN\n";
$r = fbird_execute($stmt, 3, null, 'null');
var_dump($r !== false);

// Test 4: explicit NULL for VARCHAR alongside non-NULL BOOLEAN
echo "Test 4: NULL varchar alongside BOOLEAN\n";
$r = fbird_execute($stmt, 4, true, null);
var_dump($r !== false);

fbird_commit($dbh);

// Verify roundtrip
$q = fbird_query($dbh, 'SELECT ID, B, V FROM BIND_BOOL_COV ORDER BY ID');
while ($row = fbird_fetch_row($q)) {
    // Normalize: Firebird returns '1'/'0' or true/false depending on version
    $b = $row[1];
    if ($b === '1' || $b === true || $b === 1) $b = 'true';
    elseif ($b === '0' || $b === false || $b === 0) $b = 'false';
    else $b = 'null';
    echo "ID={$row[0]} B=$b V=" . var_export($row[2], true) . "\n";
}
fbird_free_result($q);

// Cleanup: free prepared stmt and commit implicit tx before DDL.
// Use @ on final commit — prepared statement handle may still hold the table.
@fbird_commit($dbh);
@fbird_query($dbh, 'DROP TABLE BIND_BOOL_COV');
@fbird_commit($dbh);
fbird_close($dbh);

echo "Done\n";
?>
--EXPECTF--
Test 1: bind true
bool(true)
Test 2: bind false
bool(true)
Test 3: bind NULL to BOOLEAN
bool(true)
Test 4: NULL varchar alongside BOOLEAN
bool(true)
ID=1 B=true V='yes'
ID=2 B=false V='no'
ID=3 B=null V='null'
ID=4 B=true V=NULL
Done
--CLEAN--
<?php require_once __DIR__ . '/../clean.inc'; ?>

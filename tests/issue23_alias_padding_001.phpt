--TEST--
Issue #23: Column alias trimming — CHAR-padded aliases are returned without trailing spaces
--SKIPIF--
<?php
include("skipif.inc");
if (PHP_VERSION_ID < 80200) die('skip PHP < 8.2');
?>
--FILE--
<?php
/*
 * Test for Issue #23: Column alias padding
 *
 * Firebird may return CHAR-type column aliases with trailing space padding.
 * The extension must trim these so keys like "COL   " do not appear.
 */

require_once('config.inc');
require("firebird.inc");

$db = fbird_connect($test_base);
if (!$db) die("Could not connect\n");

@fbird_query($db, "DROP TABLE ISSUE23_TEST");
@fbird_commit($db);

fbird_query($db, "CREATE TABLE ISSUE23_TEST (A INTEGER, B INTEGER)");
fbird_commit($db);

fbird_query($db, "INSERT INTO ISSUE23_TEST (A, B) VALUES (1, 2)");
fbird_commit($db);

$result = fbird_query($db, "SELECT A AS MYALIAS, B AS OTHER FROM ISSUE23_TEST");
if (!$result) die("Query failed: " . fbird_errmsg() . "\n");

$row = fbird_fetch_assoc($result);
if (!$row) die("No rows returned\n");

$has_trailing_spaces = false;
foreach (array_keys($row) as $key) {
    if (preg_match('/\s+$/', $key)) {
        $has_trailing_spaces = true;
        echo "FAIL: Key has trailing spaces: [$key]\n";
    }
}
if (!$has_trailing_spaces) echo "OK: No trailing spaces in keys\n";

echo "MYALIAS: " . (isset($row['MYALIAS']) ? "present" : "MISSING") . "\n";
echo "OTHER: "   . (isset($row['OTHER'])   ? "present" : "MISSING") . "\n";

fbird_free_result($result);
fbird_commit($db);
@fbird_query($db, "DROP TABLE ISSUE23_TEST");
@fbird_commit($db);
fbird_close($db);
echo "Test complete\n";
?>
--EXPECT--
OK: No trailing spaces in keys
MYALIAS: present
OTHER: present
Test complete

--CLEAN--
<?php
require_once 'firebird.inc';
$db = @fbird_connect($test_base, $user, $password);
if ($db) {
    @fbird_query($db, "DROP TABLE ISSUE23_TEST");
    @fbird_close($db);
}
?>

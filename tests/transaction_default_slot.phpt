--TEST--
Default transaction sentinel head slot pattern (issues #523, #541)
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

/* Regression test for the sentinel head node pattern in tr_list.
 *
 * The first node in the connection's transaction list (tr_list[0]) is
 * reserved for the default transaction. Explicit transactions are
 * appended at index >= 1. _php_fbird_commit_link() uses this positional
 * invariant to distinguish:
 *   - Default tx (index 0): commit + efree (not a registered le_trans resource)
 *   - Explicit tx (index >0): rollback + leave to le_trans destructor
 *
 * Removing the placeholder (trans=NULL sentinel) causes use-after-free:
 * explicit transactions land at index 0 and get efree()'d directly,
 * then the le_trans destructor touches freed memory. See issues #523, #541.
 *
 * This test covers:
 *   1. fbird_trans_start() with no prior default transaction
 *   2. fbird_query() creates default transaction
 *   3. Both default + explicit transaction coexist
 *   4. pconnect cached connection (issue119 regression)
 *   5. Multiple trans_start cycles (issue307 regression)
 */

echo "Test 1: fbird_trans_start() with no prior default transaction\n";
$conn = fbird_connect($test_base);
if (!$conn) die("connect failed: " . fbird_errmsg() . "\n");
$tx = fbird_trans_start($conn, [FBIRD_WRITE]);
var_dump(is_resource($tx) || (is_object($tx) && $tx instanceof Firebird\Transaction));
fbird_commit($tx);

echo "\nTest 2: fbird_query() creates default transaction\n";
$q = fbird_query($conn, "SELECT 1 FROM rdb\$database");
$row = fbird_fetch_row($q);
var_dump($row);
fbird_free_result($q);

echo "\nTest 3: Both default + explicit transaction coexist\n";
$q2 = fbird_query($conn, "SELECT 2 FROM rdb\$database");
$tx2 = fbird_trans_start($conn, [FBIRD_WRITE]);
var_dump(is_resource($tx2) || (is_object($tx2) && $tx2 instanceof Firebird\Transaction));
fbird_commit($tx2);
fbird_free_result($q2);

fbird_close($conn);

echo "\nTest 4: pconnect cached connection (issue119 regression)\n";
$conn1 = fbird_pconnect($test_base, $user, $password);
if (!$conn1) die("pconnect 1 failed: " . fbird_errmsg() . "\n");
$tr1 = fbird_trans_start($conn1, [FBIRD_WRITE]);
var_dump(is_resource($tr1) || (is_object($tr1) && $tr1 instanceof Firebird\Transaction));
fbird_commit($tr1);
fbird_close($conn1);

$conn2 = fbird_pconnect($test_base, $user, $password);
if (!$conn2) die("pconnect 2 failed: " . fbird_errmsg() . "\n");
$tr2 = fbird_trans_start($conn2, [FBIRD_WRITE]);
var_dump(is_resource($tr2) || (is_object($tr2) && $tr2 instanceof Firebird\Transaction));
fbird_commit($tr2);
fbird_close($conn2);

echo "\nTest 5: Multiple trans_start cycles (issue307 regression)\n";
$conn3 = fbird_connect($test_base);
for ($i = 0; $i < 3; $i++) {
    $t = fbird_trans_start($conn3, [FBIRD_WRITE]);
    if (!$t) die("trans_start cycle $i failed: " . fbird_errmsg() . "\n");
    fbird_commit($t);
}
echo "3 cycles OK\n";
fbird_close($conn3);

echo "\nDone\n";
?>
--EXPECTF--
Test 1: fbird_trans_start() with no prior default transaction
bool(true)

Test 2: fbird_query() creates default transaction
array(1) {
  [0]=>
  int(1)
}

Test 3: Both default + explicit transaction coexist
bool(true)

Test 4: pconnect cached connection (issue119 regression)
bool(true)
bool(true)

Test 5: Multiple trans_start cycles (issue307 regression)
3 cycles OK

Done

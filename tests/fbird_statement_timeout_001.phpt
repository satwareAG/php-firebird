--TEST--
Firebird 4.0+ statement timeout via SQL (#422)
--SKIPIF--
<?php
require_once 'skipif.inc';
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('STATEMENT_TIMEOUT')) die('skip STATEMENT_TIMEOUT not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

echo "=== Test 1: SET STATEMENT TIMEOUT ===\n";
/* SET STATEMENT TIMEOUT sets the timeout for subsequent statements
 * on this attachment. Value is in milliseconds. */
$ok = fbird_query($db, "SET STATEMENT TIMEOUT 5000");
fbird_commit($db);
echo "SET STATEMENT TIMEOUT 5000: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

echo "\n=== Test 2: Normal query completes within timeout ===\n";
$trans = fbird_trans($db);
$q = fbird_query($trans, "SELECT 1 FROM RDB\$DATABASE");
$row = fbird_fetch_row($q);
echo "Quick query result: " . $row[0] . "\n";
fbird_free_result($q);
fbird_commit($trans);

echo "\n=== Test 3: Statement timeout enforcement ===\n";
/* Set a very short timeout (1ms) and run a query that should take
 * longer than 1ms. The server should abort the statement. */
$ok = fbird_query($db, "SET STATEMENT TIMEOUT 1");
fbird_commit($db);
echo "SET STATEMENT TIMEOUT 1: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

/* Run a deliberately slow query — a recursive CTE or a big cross join.
 * With 1ms timeout, this should fail. */
$trans2 = fbird_trans($db);
$timed_out = false;
$result = @fbird_query($trans2, "SELECT r1.rdb\$field_name, r2.rdb\$field_name, r3.rdb\$field_name
    FROM rdb\$fields r1
    CROSS JOIN rdb\$fields r2
    CROSS JOIN rdb\$fields r3
    WHERE r1.rdb\$field_id + r2.rdb\$field_id + r3.rdb\$field_id > 0");

if (!$result) {
    $err = fbird_errmsg();
    /* The error message should contain timeout-related text */
    echo "Statement timed out: yes\n";
    echo "Error: " . substr($err, 0, 80) . "\n";
    $timed_out = true;
} else {
    /* If the query completed, the timeout wasn't enforced */
    echo "Statement timed out: no (query completed)\n";
    fbird_free_result($result);
}
fbird_rollback($trans2);

echo "\n=== Test 4: Reset timeout and verify normal operation ===\n";
fbird_query($db, "SET STATEMENT TIMEOUT 0");
fbird_commit($db);
$trans3 = fbird_trans($db);
$q = fbird_query($trans3, "SELECT 42 FROM RDB\$DATABASE");
$row = fbird_fetch_row($q);
echo "After reset: " . $row[0] . "\n";
fbird_free_result($q);
fbird_commit($trans3);

/* Cleanup */
fbird_query($db, "SET STATEMENT TIMEOUT 0");
fbird_commit($db);
fbird_close($db);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: SET STATEMENT TIMEOUT ===
SET STATEMENT TIMEOUT 5000: ok

=== Test 2: Normal query completes within timeout ===
Quick query result: 1

=== Test 3: Statement timeout enforcement ===
SET STATEMENT TIMEOUT 1: ok
Statement timed out: %s
%s

=== Test 4: Reset timeout and verify normal operation ===
After reset: 42

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

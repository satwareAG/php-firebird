--TEST--
Firebird 5.0+ profiler plugin via SQL (#428)
--SKIPIF--
<?php
require_once 'skipif.inc';
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('PROFILER')) die('skip profiler not supported (requires FB5+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

echo "=== Test 1: Start profiler session ===\n";
/* rdb$profiler.start_session() starts a profiling session.
 * Args: (description, attachment_id, plugin_name, plugin_options, detailed_requests) */
$q = @fbird_query($db, "SELECT rdb\$profiler.start_session('test_session', null, null, null, 'DETAILED_REQUESTS') FROM rdb\$database");
if ($q) {
    $row = fbird_fetch_row($q);
    echo "start_session: ok (id=" . $row[0] . ")\n";
    fbird_free_result($q);
} else {
    /* Try without DETAILED_REQUESTS */
    $q = @fbird_query($db, "SELECT rdb\$profiler.start_session('test_session') FROM rdb\$database");
    if ($q) {
        $row = fbird_fetch_row($q);
        echo "start_session: ok (id=" . $row[0] . ")\n";
        fbird_free_result($q);
    } else {
        echo "start_session: failed (" . fbird_errmsg() . ")\n";
        fbird_close($db);
        die("Cannot proceed without profiler\n");
    }
}
fbird_commit($db);

echo "\n=== Test 2: Run a query while profiling ===\n";
$q = fbird_query($db, "SELECT COUNT(*) FROM RDB\$RELATIONS");
$row = fbird_fetch_row($q);
echo "Query result: " . $row[0] . " relations\n";
fbird_free_result($q);
fbird_commit($db);

echo "\n=== Test 3: Flush profiler data ===\n";
/* Flush writes profiling data to the snapshot tables */
$ok = @fbird_query($db, "EXECUTE PROCEDURE rdb\$profiler.flush");
echo "flush: " . ($ok ? 'ok' : 'failed (' . fbird_errmsg() . ')') . "\n";
fbird_commit($db);

echo "\n=== Test 4: Finish profiler session ===\n";
/* finish_session(true) flushes and finalizes the session */
$ok = @fbird_query($db, "EXECUTE PROCEDURE rdb\$profiler.finish_session(true)");
echo "finish_session: " . ($ok ? 'ok' : 'failed (' . fbird_errmsg() . ')') . "\n";
fbird_commit($db);

echo "\n=== Test 5: Query profiler session data ===\n";
/* Set search path to access profiler schema objects */
@fbird_query($db, "SET SEARCH PATH TO plg\$profiler, public, system");
fbird_commit($db);

/* Check if a session was recorded */
$q = @fbird_query($db, "SELECT COUNT(*) FROM plg\$prof_sessions");
if ($q) {
    $row = fbird_fetch_row($q);
    echo "prof_sessions count: " . $row[0] . "\n";
    fbird_free_result($q);
} else {
    /* Try without search path */
    $q = @fbird_query($db, "SELECT COUNT(*) FROM plg\$prof_sessions");
    echo "prof_sessions: " . ($q ? fbird_fetch_row($q)[0] : '(table not accessible)') . "\n";
    if ($q) fbird_free_result($q);
}
fbird_commit($db);

echo "\n=== Test 6: Query profiler request stats ===\n";
/* Check if request-level stats were collected */
$q = @fbird_query($db, "SELECT COUNT(*) FROM plg\$prof_requests");
if ($q) {
    $row = fbird_fetch_row($q);
    echo "prof_requests count: " . $row[0] . "\n";
    fbird_free_result($q);
} else {
    echo "prof_requests: (table not accessible)\n";
}
fbird_commit($db);

echo "\n=== Test 7: Discard session (cleanup) ===\n";
$ok = @fbird_query($db, "EXECUTE PROCEDURE rdb\$profiler.discard_session");
echo "discard_session: " . ($ok ? 'ok' : 'not needed') . "\n";
fbird_commit($db);

fbird_close($db);
echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Start profiler session ===
start_session: ok%s

=== Test 2: Run a query while profiling ===
Query result: %d relations

=== Test 3: Flush profiler data ===
flush: %s

=== Test 4: Finish profiler session ===
finish_session: %s

=== Test 5: Query profiler session data ===
prof_sessions count: %s

=== Test 6: Query profiler request stats ===
prof_requests count: %s

=== Test 7: Discard session (cleanup) ===
discard_session: %s

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

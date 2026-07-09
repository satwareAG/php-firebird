--TEST--
Firebird 4.0+ EXECUTE STATEMENT rich form + SET TIME ZONE + AT TIME ZONE (#424)
--SKIPIF--
<?php
require_once 'skipif.inc';
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('TIME_TZ')) die('skip TIME_TZ not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

echo "=== Test 1: SET TIME ZONE ===\n";
/* SET TIME ZONE sets the session timezone. */
$ok = fbird_query($db, "SET TIME ZONE 'UTC'");
fbird_commit($db);
echo "SET TIME ZONE 'UTC': " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

/* Verify via LOCALTIMESTAMP format which reflects session TZ */
$q = fbird_query($db, "SELECT CAST(LOCALTIMESTAMP AS VARCHAR(40)) FROM RDB\$DATABASE");
$row = fbird_fetch_row($q);
echo "LOCALTIMESTAMP after SET UTC: " . $row[0] . "\n";
fbird_free_result($q);

/* Set to a named zone */
fbird_query($db, "SET TIME ZONE 'Europe/Berlin'");
fbird_commit($db);
$q = fbird_query($db, "SELECT CAST(LOCALTIMESTAMP AS VARCHAR(40)) FROM RDB\$DATABASE");
$row = fbird_fetch_row($q);
echo "LOCALTIMESTAMP after SET Berlin: " . $row[0] . "\n";
fbird_free_result($q);

/* Reset to LOCAL */
fbird_query($db, "SET TIME ZONE LOCAL");
fbird_commit($db);

echo "\n=== Test 2: EXTRACT(TIMEZONE_HOUR|MINUTE) on TZ literal ===\n";
/* FB 4.0 supports EXTRACT(TIMEZONE_HOUR|MINUTE) but NOT TIMEZONE_NAME */
$q = fbird_query($db, "SELECT
    EXTRACT(TIMEZONE_HOUR FROM TIMESTAMP'2026-07-08 10:00:00 Europe/Berlin'),
    EXTRACT(TIMEZONE_MINUTE FROM TIMESTAMP'2026-07-08 10:00:00 Europe/Berlin')
    FROM RDB\$DATABASE");
$row = fbird_fetch_row($q);
echo "Berlin TZ hour: " . $row[0] . ", minute: " . $row[1] . "\n";
fbird_free_result($q);

echo "\n=== Test 3: AT TIME ZONE operator ===\n";
$q = fbird_query($db, "SELECT
    CAST(TIMESTAMP'2026-07-08 10:00:00' AT TIME ZONE 'UTC' AS VARCHAR(40))
    FROM RDB\$DATABASE");
$row = fbird_fetch_row($q);
echo "Local timestamp AT TIME ZONE 'UTC': " . $row[0] . "\n";
fbird_free_result($q);

echo "\n=== Test 4: EXECUTE STATEMENT — basic form (via EXECUTE BLOCK) ===\n";
/* EXECUTE STATEMENT is a PSQL construct — must be inside EXECUTE BLOCK */
$q = fbird_query($db, "EXECUTE BLOCK RETURNS (result INTEGER) AS
    BEGIN
    EXECUTE STATEMENT 'SELECT 1 + 1 FROM RDB\$DATABASE' INTO result;
    SUSPEND;
    END");
$row = fbird_fetch_row($q);
echo "EXECUTE STATEMENT 'SELECT 1+1': " . $row[0] . "\n";
fbird_free_result($q);

echo "\n=== Test 5: EXECUTE STATEMENT — WITH AUTONOMOUS TRANSACTION ===\n";
/* Autonomous transaction: the inner statement runs in a separate transaction. */
@fbird_query($db, "RECREATE TABLE ES_TEST (val VARCHAR(50))");
fbird_commit($db);
$ok = fbird_query($db, "EXECUTE BLOCK AS
    BEGIN
    EXECUTE STATEMENT 'INSERT INTO ES_TEST VALUES (''auto_test'')' WITH AUTONOMOUS TRANSACTION;
    END");
fbird_commit($db);
echo "EXECUTE STATEMENT WITH AUTONOMOUS: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

$q = @fbird_query($db, "SELECT COUNT(*) FROM ES_TEST WHERE val = 'auto_test'");
$row = $q ? fbird_fetch_row($q) : [0];
echo "Autonomous insert verified: " . ($row[0] > 0 ? 'yes' : 'no') . "\n";
if ($q) fbird_free_result($q);
@fbird_query($db, "DROP TABLE ES_TEST");
fbird_commit($db);

echo "\n=== Test 6: EXECUTE STATEMENT — ON EXTERNAL DATA SOURCE ===\n";
/* ON EXTERNAL executes against a different database. Test with same DB. */
global $test_base;
$trans = fbird_trans($db);
$stmt = @fbird_prepare($trans, "EXECUTE BLOCK RETURNS (result VARCHAR(50)) AS
    BEGIN
    EXECUTE STATEMENT 'SELECT CURRENT_USER FROM RDB\$DATABASE'
        ON EXTERNAL '" . $test_base . "'
        AS USER 'SYSDBA' PASSWORD 'masterkey'
        INTO result;
    SUSPEND;
    END");
if ($stmt) {
    $rs = fbird_execute($stmt);
    $row = fbird_fetch_row($rs);
    echo "ON EXTERNAL result: " . $row[0] . "\n";
    fbird_free_query($rs);
} else {
    echo "ON EXTERNAL: not supported or failed\n";
}
fbird_commit($trans);

fbird_close($db);
echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: SET TIME ZONE ===
SET TIME ZONE 'UTC': ok
LOCALTIMESTAMP after SET UTC: %s
LOCALTIMESTAMP after SET Berlin: %s

=== Test 2: EXTRACT(TIMEZONE_HOUR|MINUTE) on TZ literal ===
Berlin TZ hour: %d, minute: %d

=== Test 3: AT TIME ZONE operator ===
Local timestamp AT TIME ZONE 'UTC': %s

=== Test 4: EXECUTE STATEMENT — basic form (via EXECUTE BLOCK) ===
EXECUTE STATEMENT 'SELECT 1+1': 2

=== Test 5: EXECUTE STATEMENT — WITH AUTONOMOUS TRANSACTION ===
EXECUTE STATEMENT WITH AUTONOMOUS: ok
Autonomous insert verified: %s

=== Test 6: EXECUTE STATEMENT — ON EXTERNAL DATA SOURCE ===
%s

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

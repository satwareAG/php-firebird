--TEST--
Firebird 4.0+ packages and SQL SECURITY clause (#423)
--SKIPIF--
<?php
require_once 'skipif.inc';
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('PACKAGES')) die('skip packages not supported (requires FB4+)');
if (!fb_server_supports('SQL_SECURITY')) die('skip SQL SECURITY not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

/* Cleanup */
@fbird_query($db, 'DROP PACKAGE FB_PKG_TEST');
@fbird_query($db, 'DROP PROCEDURE FB_SS_DEF');
@fbird_query($db, 'DROP PROCEDURE FB_SS_INV');
fbird_commit($db);

echo "=== Test 1: CREATE PACKAGE + PACKAGE BODY ===\n";
$ok = fbird_query($db, "CREATE PACKAGE FB_PKG_TEST AS
    BEGIN
    PROCEDURE get_value RETURNS (result INTEGER);
    END");
fbird_commit($db);
echo "CREATE PACKAGE: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

$ok = fbird_query($db, "CREATE PACKAGE BODY FB_PKG_TEST AS
    BEGIN
    PROCEDURE get_value RETURNS (result INTEGER)
    AS
    BEGIN
        result = 42;
    END
    END");
fbird_commit($db);
echo "CREATE PACKAGE BODY: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

echo "\n=== Test 2: Call package procedure ===\n";
$trans = fbird_trans($db);
$stmt = fbird_prepare($trans, "EXECUTE PROCEDURE FB_PKG_TEST.GET_VALUE");
$rs = fbird_execute($stmt);
$row = fbird_fetch_row($rs);
fbird_free_query($rs);
fbird_commit($trans);
echo "Package proc returns: " . $row[0] . "\n";

echo "\n=== Test 3: RECREATE PACKAGE ===\n";
$ok = fbird_query($db, "RECREATE PACKAGE FB_PKG_TEST AS
    BEGIN
    PROCEDURE get_value RETURNS (result INTEGER);
    PROCEDURE get_double RETURNS (result INTEGER);
    END");
fbird_commit($db);
echo "RECREATE PACKAGE: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

$ok = fbird_query($db, "RECREATE PACKAGE BODY FB_PKG_TEST AS
    BEGIN
    PROCEDURE get_value RETURNS (result INTEGER)
    AS BEGIN result = 99; END

    PROCEDURE get_double RETURNS (result INTEGER)
    AS BEGIN result = 198; END
    END");
fbird_commit($db);
echo "RECREATE PACKAGE BODY: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

$trans = fbird_trans($db);
$stmt = fbird_prepare($trans, "EXECUTE PROCEDURE FB_PKG_TEST.GET_DOUBLE");
$rs = fbird_execute($stmt);
$row = fbird_fetch_row($rs);
fbird_free_query($rs);
fbird_commit($trans);
echo "New proc returns: " . $row[0] . "\n";

echo "\n=== Test 4: SQL SECURITY DEFINER vs INVOKER ===\n";
fbird_query($db, "CREATE PROCEDURE FB_SS_DEF SQL SECURITY DEFINER AS BEGIN END");
fbird_query($db, "CREATE PROCEDURE FB_SS_INV SQL SECURITY INVOKER AS BEGIN END");
fbird_commit($db);
echo "SQL SECURITY DEFINER: created\n";
echo "SQL SECURITY INVOKER: created\n";

$trans = fbird_trans($db);
fbird_query($trans, "EXECUTE PROCEDURE FB_SS_DEF");
fbird_query($trans, "EXECUTE PROCEDURE FB_SS_INV");
fbird_commit($trans);
echo "Both procedures execute: ok\n";

echo "\n=== Test 5: ALTER DATABASE SET DEFAULT SQL SECURITY ===\n";
$ok = fbird_query($db, "ALTER DATABASE SET DEFAULT SQL SECURITY DEFINER");
fbird_commit($db);
echo "SET DEFAULT DEFINER: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

$ok = fbird_query($db, "ALTER DATABASE SET DEFAULT SQL SECURITY INVOKER");
fbird_commit($db);
echo "SET DEFAULT INVOKER: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

/* Reset to default */
fbird_query($db, "ALTER DATABASE SET DEFAULT SQL SECURITY DEFINER");
fbird_commit($db);

echo "\n=== Test 6: DROP PACKAGE ===\n";
$ok = fbird_query($db, "DROP PACKAGE FB_PKG_TEST");
fbird_commit($db);
echo "DROP PACKAGE: " . ($ok ? 'ok' : 'failed: ' . fbird_errmsg()) . "\n";

fbird_query($db, "DROP PROCEDURE FB_SS_DEF");
fbird_query($db, "DROP PROCEDURE FB_SS_INV");
fbird_commit($db);

fbird_close($db);
echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: CREATE PACKAGE + PACKAGE BODY ===
CREATE PACKAGE: ok
CREATE PACKAGE BODY: ok

=== Test 2: Call package procedure ===
Package proc returns: 42

=== Test 3: RECREATE PACKAGE ===
RECREATE PACKAGE: ok
RECREATE PACKAGE BODY: ok
New proc returns: 198

=== Test 4: SQL SECURITY DEFINER vs INVOKER ===
SQL SECURITY DEFINER: created
SQL SECURITY INVOKER: created
Both procedures execute: ok

=== Test 5: ALTER DATABASE SET DEFAULT SQL SECURITY ===
SET DEFAULT DEFINER: ok
SET DEFAULT INVOKER: ok

=== Test 6: DROP PACKAGE ===
DROP PACKAGE: ok

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

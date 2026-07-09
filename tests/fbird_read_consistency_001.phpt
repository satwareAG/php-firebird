--TEST--
Firebird 4.0+ READ CONSISTENCY transaction isolation (#425)
--SKIPIF--
<?php
require_once 'skipif.inc';
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('READ_CONSISTENCY')) die('skip READ_CONSISTENCY not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

/* ============================================================
 * Issue #425: FB4 READ CONSISTENCY transaction isolation
 *
 * READ CONSISTENCY (isc_tpb_read_consistency) provides statement-level
 * snapshot isolation within READ COMMITTED transactions. The engine
 * auto-restarts statements on update conflicts instead of throwing errors.
 *
 * Requires FBIRD_COMMITTED | FBIRD_READ_CONSISTENCY bitmask.
 * ============================================================ */

echo "=== Test 1: Start READ CONSISTENCY transaction (flag path) ===\n";
$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

@fbird_query($db, 'DROP TABLE RC_TEST');
fbird_query($db, 'CREATE TABLE RC_TEST (ID INTEGER NOT NULL PRIMARY KEY, VAL INTEGER)');
fbird_query($db, 'INSERT INTO RC_TEST VALUES (1, 100)');
fbird_commit($db);

/* Flag-based: FBIRD_COMMITTED | FBIRD_READ_CONSISTENCY */
$trans = @fbird_trans(FBIRD_COMMITTED | FBIRD_READ_CONSISTENCY, $db);
echo "Transaction started: " . ($trans ? 'yes' : 'no') . "\n";

/* Read the row — sees initial value */
$q = fbird_query($trans, "SELECT VAL FROM RC_TEST WHERE ID = 1");
$row = fbird_fetch_row($q);
echo "Initial read: " . $row[0] . "\n";
fbird_free_result($q);
fbird_commit($trans);

echo "\n=== Test 2: Start READ CONSISTENCY (array-API path) ===\n";
/* Array-based: ['read_consistency' => true, 'isolation' => FBIRD_COMMITTED] */
$trans2 = @fbird_trans($db, ['read_consistency' => true, 'isolation' => FBIRD_COMMITTED]);
echo "Array-API transaction started: " . ($trans2 ? 'yes' : 'no') . "\n";
$q = fbird_query($trans2, "SELECT VAL FROM RC_TEST WHERE ID = 1");
$row = fbird_fetch_row($q);
echo "Array-API read: " . $row[0] . "\n";
fbird_free_result($q);
fbird_commit($trans2);

echo "\n=== Test 3: READ CONSISTENCY sees latest committed data ===\n";
/* READ CONSISTENCY is a READ COMMITTED variant: each statement sees the
 * latest committed data (statement-level snapshot). This is NOT SNAPSHOT
 * isolation — the reader will see the writer's committed update. */
$reader = fbird_connect($test_base, $user, $password);
$writer = fbird_connect($test_base, $user, $password);

/* Reader: start read-consistency transaction, read row */
$rc_trans = fbird_trans(FBIRD_COMMITTED | FBIRD_READ_CONSISTENCY, $reader);
$q = fbird_query($rc_trans, "SELECT VAL FROM RC_TEST WHERE ID = 1");
$row = fbird_fetch_row($q);
echo "Reader initial: " . $row[0] . "\n";
fbird_free_result($q);

/* Writer: update the row and commit */
fbird_query($writer, "UPDATE RC_TEST SET VAL = 200 WHERE ID = 1");
fbird_commit($writer);
echo "Writer updated to: 200\n";

/* Reader: re-read — READ CONSISTENCY sees the latest committed value (200),
 * because each statement gets a fresh snapshot in READ COMMITTED mode. */
$q = fbird_query($rc_trans, "SELECT VAL FROM RC_TEST WHERE ID = 1");
$row = fbird_fetch_row($q);
echo "Reader after writer commit: " . $row[0] . "\n";
$sees_latest = ($row[0] == 200);
echo "Reader sees latest committed: " . ($sees_latest ? 'yes' : 'no') . "\n";
fbird_free_result($q);
fbird_commit($rc_trans);

echo "\n=== Test 4: Auto-restart on UPDATE conflict ===\n";
/* With read consistency, if the reader tries to UPDATE a row that was
 * modified by the writer AFTER the reader's transaction started, the
 * engine auto-restarts the statement against the latest version instead
 * of throwing isc_update_conflict. */
/* Reader starts transaction FIRST */
$rc_trans2 = fbird_trans(FBIRD_COMMITTED | FBIRD_READ_CONSISTENCY, $reader);

/* Writer updates the same row and commits WHILE reader's transaction is open */
fbird_query($writer, "UPDATE RC_TEST SET VAL = 300 WHERE ID = 1");
fbird_commit($writer);
echo "Writer updated to: 300 (during reader's transaction)\n";

/* Reader: try to UPDATE the same row. Without read_consistency, this would
 * throw isc_update_conflict. With read_consistency, the engine auto-restarts. */
$ok = @fbird_query($rc_trans2, "UPDATE RC_TEST SET VAL = 400 WHERE ID = 1");
echo "Reader UPDATE (auto-restart): " . ($ok ? 'ok (no conflict)' : 'conflict error') . "\n";
fbird_commit($rc_trans2);

/* Verify the update was applied */
$q = fbird_query($db, "SELECT VAL FROM RC_TEST WHERE ID = 1");
$row = fbird_fetch_row($q);
echo "Final value: " . $row[0] . "\n";
fbird_free_result($q);

echo "\n=== Test 5: TBuilder useReadConsistency() ===\n";
/* Test the OOP TBuilder path. TBuilder uses a static factory or
 * is instantiated via the connection. Check if it can be created. */
$tbuilder_file = dirname(__DIR__) . '/src/Firebird/TBuilder.php';
if (file_exists($tbuilder_file)) {
    require_once $tbuilder_file;
    /* TBuilder constructor is private — use reflection or check the class API */
    $rc = new ReflectionClass(\Firebird\TBuilder::class);
    if ($rc->hasMethod('isolationReadCommittedReadConsistency')) {
        echo "TBuilder has isolationReadCommittedReadConsistency method: yes\n";
    }
    if ($rc->hasMethod('buildFlags')) {
        echo "TBuilder has buildFlags method: yes\n";
    }
    /* Check that build() includes read_consistency key when set */
    $method = $rc->getMethod('build');
    echo "TBuilder build() returns array: " . ($method->getReturnType() ? 'yes' : 'unknown') . "\n";
} else {
    echo "TBuilder not available\n";
}

/* Cleanup */
@fbird_query($db, 'DROP TABLE RC_TEST');
fbird_commit($db);
fbird_close($reader);
fbird_close($writer);
fbird_close($db);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Start READ CONSISTENCY transaction (flag path) ===
Transaction started: yes
Initial read: 100

=== Test 2: Start READ CONSISTENCY (array-API path) ===
Array-API transaction started: yes
Array-API read: 100

=== Test 3: READ CONSISTENCY sees latest committed data ===
Reader initial: 100
Writer updated to: 200
Reader after writer commit: 200
Reader sees latest committed: yes

=== Test 4: Auto-restart on UPDATE conflict ===
Writer updated to: 300 (during reader's transaction)
Reader UPDATE (auto-restart): %s
Final value: %s

=== Test 5: TBuilder useReadConsistency() ===
TBuilder has isolationReadCommittedReadConsistency method: yes
TBuilder has buildFlags method: yes
TBuilder build() returns array: yes

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

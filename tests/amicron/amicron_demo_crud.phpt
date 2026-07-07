--TEST--
amicron-platform: schema CRUD against amicron-demo.fdb
--CREDITS--
v12.1.0 M3-3 (#354) - amicron-platform customer integration test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
/**
 * Test SELECT/INSERT/UPDATE/DELETE across the 5 major Amicron tables
 * on the real amicron-demo.fdb (FB 3.0.13, 59 ADRESSEN, 257 AUFTRAG,
 * 301 ARTIKEL, 1039 ATRPOS, 162 KONTAKTE rows).
 *
 * Uses AMICRON03/klopf (public demo creds from amicron-software.de).
 * Test rows use LFDNR 999990-999999 and AUFTRAGNR 'TEST-*' for cleanup.
 */
$DB = 'localhost/3050:/var/lib/firebird/data/amicron-demo.fdb';
$USER = 'AMICRON03';
$PASS = 'klopf';
$CHARSET = 'ISO8859_1';

echo "=== amicron schema CRUD ===\n";

// --- Connect via procedural API ---
$conn = fbird_connect($DB, $USER, $PASS, $CHARSET);
if (!$conn) {
    echo "FAIL connect: " . fbird_errmsg() . "\n";
    exit(1);
}
echo "OK fbird_connect to amicron-demo.fdb\n";

// --- 1. SELECT counts on all 5 tables ---
$tables = ['ADRESSEN' => 59, 'AUFTRAG' => 257, 'ARTIKEL' => 301, 'ATRPOS' => 1039, 'KONTAKTE' => 162];
foreach ($tables as $table => $expected_min) {
    $rs = fbird_query($conn, "SELECT COUNT(*) FROM $table");
    $row = fbird_fetch_row($rs);
    $count = (int)$row[0];
    if ($count >= $expected_min) {
        echo "OK $table: $count rows (>= $expected_min)\n";
    } else {
        echo "FAIL $table: $count rows (expected >= $expected_min)\n";
    }
    fbird_free_result($rs);
}

// --- 2. INSERT with valid FK (AUFTRAG -> ADRESSEN via KUNDENLFDNR_FK) ---
// LFDNR 1948 exists in ADRESSEN (verified earlier)
$trx = fbird_trans($conn);
$rs = fbird_query($trx, "INSERT INTO AUFTRAG (LFDNR, AUFTRAGNR, ART, DATUM, KUNDENLFDNR) VALUES (999990, 'TEST-CRUD', 'X', '2026-07-07', 1948)");
if ($rs) {
    echo "OK INSERT AUFTRAG with valid FK (KUNDENLFDNR=1948)\n";
} else {
    echo "FAIL INSERT: " . fbird_errmsg() . "\n";
}
fbird_rollback($trx);
echo "OK rollback (test row not persisted)\n";

// --- 3. INSERT with invalid FK should fail ---
$trx = fbird_trans($conn);
$rs = @fbird_query($trx, "INSERT INTO AUFTRAG (LFDNR, AUFTRAGNR, ART, DATUM, KUNDENLFDNR) VALUES (999991, 'TEST-BADFK', 'X', '2026-07-07', 1)");
if ($rs === false) {
    echo "OK INSERT with invalid FK (KUNDENLFDNR=1) rejected\n";
} else {
    echo "FAIL INSERT with invalid FK should have been rejected\n";
    fbird_rollback($trx);
}

// --- 4. PDO path: same queries via PDO ---
$dsn = "fbird:host=localhost;dbname=/var/lib/firebird/data/amicron-demo.fdb;charset=ISO8859_1";
$pdo = new PDO($dsn, $USER, $PASS, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);

// SELECT via PDO
$count = (int)$pdo->query("SELECT COUNT(*) FROM ADRESSEN")->fetchColumn();
echo "OK PDO ADRESSEN count: $count\n";

// INSERT + rollback via PDO
$pdo->beginTransaction();
$pdo->exec("INSERT INTO AUFTRAG (LFDNR, AUFTRAGNR, ART, DATUM, KUNDENLFDNR) VALUES (999992, 'TEST-PDO', 'X', '2026-07-07', 1948)");
$pdo->rollBack();
echo "OK PDO INSERT + rollback\n";

// --- 5. Cross-driver consistency ---
$proc_count = (int)fbird_fetch_row(fbird_query($conn, "SELECT COUNT(*) FROM ARTIKEL"))[0];
$pdo_count = (int)$pdo->query("SELECT COUNT(*) FROM ARTIKEL")->fetchColumn();
echo "OK cross-driver: proc=$proc_count pdo=$pdo_count " . ($proc_count === $pdo_count ? "MATCH" : "MISMATCH") . "\n";

fbird_close($conn);
$pdo = null;
echo "OK disconnect\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php
$conn = @fbird_connect('localhost/3050:/var/lib/firebird/data/amicron-demo.fdb', 'AMICRON03', 'klopf', 'ISO8859_1');
if ($conn) {
    foreach (['TEST-CRUD', 'TEST-BADFK', 'TEST-PDO'] as $nr) {
        @fbird_query($conn, "DELETE FROM AUFTRAG WHERE AUFTRAGNR = '$nr'");
    }
    @fbird_query($conn, "DELETE FROM AUFTRAG WHERE LFDNR IN (999990, 999991, 999992)");
    fbird_commit($conn);
    fbird_close($conn);
}
?>
--EXPECTF--
=== amicron schema CRUD ===
OK fbird_connect to amicron-demo.fdb
OK ADRESSEN: %d rows (>= 59)
OK AUFTRAG: %d rows (>= 257)
OK ARTIKEL: %d rows (>= 301)
OK ATRPOS: %d rows (>= 1039)
OK KONTAKTE: %d rows (>= 162)
OK INSERT AUFTRAG with valid FK (KUNDENLFDNR=1948)
OK rollback (test row not persisted)
OK INSERT with invalid FK (KUNDENLFDNR=1) rejected
OK PDO ADRESSEN count: %d
OK PDO INSERT + rollback
OK cross-driver: proc=%d pdo=%d MATCH
OK disconnect
=== DONE ===

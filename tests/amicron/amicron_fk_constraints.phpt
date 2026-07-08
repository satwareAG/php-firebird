--TEST--
amicron-platform: FK constraint cycle (AUFTRAG -> ADRESSEN, AUFTRAG -> AUFTRAG)
--CREDITS--
v12.1.0 M3-5 (#356) - amicron-platform customer integration test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
/**
 * Test FK enforcement on the real Amicron schema:
 *   - AUFTRAG -> ADRESSEN via AUFTRAG_KUNDENLFDNR_FK
 *   - AUFTRAG -> AUFTRAG via AUFTRAG_FORTGEFUEHRTLFDNR_FK (self-ref)
 */
$DB = 'localhost/3050:/var/lib/firebird/data/amicron-demo.fdb';
$USER = 'SYSDBA';
$PASS = 'masterkey';
$CHARSET = 'ISO8859_1';

echo "=== amicron FK constraints ===\n";

// Use ERRMODE_SILENT to inspect errors without throwing
$conn = fbird_connect($DB, $USER, $PASS, $CHARSET);

// --- 1. Valid FK: INSERT with KUNDENLFDNR=1948 (exists in ADRESSEN) ---
$trx = fbird_trans($conn);
$rs = fbird_query($trx, "INSERT INTO AUFTRAG (LFDNR, AUFTRAGNR, ART, DATUM, KUNDENLFDNR) VALUES (999980, 'TEST-FK-OK', 'X', '2026-07-07', 1948)");
if ($rs) {
    echo "OK valid FK insert (KUNDENLFDNR=1948 exists in ADRESSEN)\n";
} else {
    echo "FAIL valid FK insert should succeed: " . fbird_errmsg() . "\n";
}
fbird_rollback($trx);

// --- 2. Invalid FK: INSERT with KUNDENLFDNR=1 (does NOT exist in ADRESSEN) ---
$trx = fbird_trans($conn);
$rs = @fbird_query($trx, "INSERT INTO AUFTRAG (LFDNR, AUFTRAGNR, ART, DATUM, KUNDENLFDNR) VALUES (999981, 'TEST-FK-BAD', 'X', '2026-07-07', 1)");
if ($rs === false) {
    $err = fbird_errmsg();
    echo "OK invalid FK rejected (KUNDENLFDNR=1 not in ADRESSEN): " . (strlen($err) > 0 ? 'has error' : 'no error') . "\n";
} else {
    echo "FAIL invalid FK insert should have been rejected\n";
    fbird_rollback($trx);
}

// --- 3. Self-referencing FK: AUFTRAG_FORTGEFUEHRTLFDNR_FK ---
// Insert a row that references itself (LFDNR = FORTGEFUEHRTLFDNR)
$trx = fbird_trans($conn);
$rs = fbird_query($trx, "INSERT INTO AUFTRAG (LFDNR, AUFTRAGNR, ART, DATUM, KUNDENLFDNR, FORTGEFUEHRTLFDNR) VALUES (999982, 'TEST-FK-SELF', 'X', '2026-07-07', 1948, 999982)");
if ($rs) {
    echo "OK self-ref FK insert (FORTGEFUEHRTLFDNR = own LFDNR)\n";
} else {
    echo "FAIL self-ref FK insert: " . fbird_errmsg() . "\n";
}
fbird_rollback($trx);

// --- 4. PDO path: FK violation via PDO ---
$pdo = new PDO(
    "fbird:host=localhost;dbname=/var/lib/firebird/data/amicron-demo.fdb;charset=ISO8859_1",
    $USER, $PASS,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT]
);
@$pdo->exec("INSERT INTO AUFTRAG (LFDNR, AUFTRAGNR, ART, DATUM, KUNDENLFDNR) VALUES (999983, 'TEST-FK-PDO', 'X', '2026-07-07', 1)");
$code = $pdo->errorCode();
if ($code !== '00000' && $code !== null) {
    echo "OK PDO FK violation SQLSTATE: $code\n";
} else {
    echo "FAIL PDO should have reported FK violation\n";
}

fbird_close($conn);
$pdo = null;
echo "OK disconnect\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php
$conn = @fbird_connect('localhost/3050:/var/lib/firebird/data/amicron-demo.fdb', 'SYSDBA', 'masterkey', 'ISO8859_1');
if ($conn) {
    foreach (['TEST-FK-OK', 'TEST-FK-BAD', 'TEST-FK-SELF', 'TEST-FK-PDO'] as $nr) {
        @fbird_query($conn, "DELETE FROM AUFTRAG WHERE AUFTRAGNR = '$nr'");
    }
    @fbird_query($conn, "DELETE FROM AUFTRAG WHERE LFDNR IN (999980, 999981, 999982, 999983)");
    fbird_commit($conn);
    fbird_close($conn);
}
?>
--EXPECTF--
=== amicron FK constraints ===
OK valid FK insert (KUNDENLFDNR=1948 exists in ADRESSEN)
OK invalid FK rejected (KUNDENLFDNR=1 not in ADRESSEN): has error
OK self-ref FK insert (FORTGEFUEHRTLFDNR = own LFDNR)
OK PDO FK violation SQLSTATE: %s
OK disconnect
=== DONE ===

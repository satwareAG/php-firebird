--TEST--
amicron-platform: BLOB column integration (ARTIKEL.BILD)
--CREDITS--
v12.1.0 M3-4 (#355) - amicron-platform customer integration test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
/**
 * Test BLOB read/write on the real ARTIKEL.BILD column
 * (BLOB sub_type 0, binary image data, ~105KB in demo data).
 *
 * Tests both PDO (PARAM_LOB stream) and procedural (fbird_blob_open/get) paths.
 */
$DB = 'localhost/3050:/var/lib/firebird/data/amicron-demo.fdb';
$USER = 'AMICRON03';
$PASS = 'klopf';
$CHARSET = 'ISO8859_1';

echo "=== amicron BLOB integration ===\n";

// --- 1. PDO: read BLOB via bindColumn(PARAM_LOB) + FETCH_BOUND ---
$dsn = "fbird:host=localhost;dbname=/var/lib/firebird/data/amicron-demo.fdb;charset=ISO8859_1";
$pdo = new PDO($dsn, $USER, $PASS, [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);

$stmt = $pdo->prepare("SELECT BILD FROM ARTIKEL WHERE BILD IS NOT NULL ROWS 1");
$stmt->execute();
$stmt->bindColumn(1, $blob, PDO::PARAM_LOB);
$stmt->fetch(PDO::FETCH_BOUND);
$blob_data = is_resource($blob) ? stream_get_contents($blob) : $blob;
$blob_len = strlen((string)$blob_data);
if ($blob_len > 0) {
    echo "OK PDO PARAM_LOB read: $blob_len bytes\n";
} else {
    echo "FAIL PDO PARAM_LOB read: empty or null\n";
}

// --- 2. Procedural: read same BLOB via fbird_blob_open ---
$conn = fbird_connect($DB, $USER, $PASS, $CHARSET);
$rs = fbird_query($conn, "SELECT BILD FROM ARTIKEL WHERE BILD IS NOT NULL ROWS 1");
$row = fbird_fetch_row($rs);
$blob_id = $row[0];
if ($blob_id) {
    $blob_handle = fbird_blob_open($conn, $blob_id);
    $blob_data_proc = '';
    while ($chunk = fbird_blob_get($blob_handle, 8192)) {
        $blob_data_proc .= $chunk;
    }
    fbird_blob_close($blob_handle);
    $proc_len = strlen($blob_data_proc);
    if ($proc_len > 0) {
        echo "OK fbird_blob_open+get: $proc_len bytes\n";
    } else {
        echo "FAIL fbird_blob read: empty\n";
    }
    // Cross-check: both methods should return same size
    if ($blob_len === $proc_len) {
        echo "OK cross-method byte count match: $blob_len\n";
    } else {
        echo "FAIL byte count mismatch: PDO=$blob_len proc=$proc_len\n";
    }
} else {
    echo "SKIP no BLOB found in ARTIKEL.BILD\n";
}
fbird_free_result($rs);

// --- 3. BLOB roundtrip: create via fbird_blob_create, read back ---
$trx = fbird_trans($conn);
$blob_handle = fbird_blob_create($trx);
fbird_blob_add($blob_handle, 'test blob content for roundtrip');
$blob_id_new = fbird_blob_close($blob_handle);

// Read the blob back to verify roundtrip
$blob_read = fbird_blob_open($trx, $blob_id_new);
$roundtrip_data = '';
while ($chunk = fbird_blob_get($blob_read, 8192)) {
    $roundtrip_data .= $chunk;
}
fbird_blob_close($blob_read);
fbird_rollback($trx);

if ($roundtrip_data === 'test blob content for roundtrip') {
    echo "OK BLOB create+read roundtrip: " . strlen($roundtrip_data) . " bytes, content matches\n";
} else {
    echo "FAIL BLOB roundtrip: content mismatch\n";
}

fbird_close($conn);
$pdo = null;
echo "OK disconnect\n";

echo "=== DONE ===\n";
?>
--CLEAN--
<?php
// No cleanup needed - all writes were rolled back
?>
--EXPECTF--
=== amicron BLOB integration ===
OK PDO PARAM_LOB read: %d bytes
OK fbird_blob_open+get: %d bytes
OK cross-method byte count match: %d
OK BLOB create+read roundtrip: %d bytes, content matches
OK disconnect
=== DONE ===

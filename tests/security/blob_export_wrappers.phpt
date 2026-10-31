--TEST--
SEC FR-003 (#733): fbird_blob_export() must reject URL stream wrappers (php://output, ftp://) - local path behavior unchanged
--CREDITS--
v13.4.0 security hardening train (#728) - FR-003 wire observer
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);

fbird_query($link, 'CREATE TABLE blob_export_t (id INTEGER, payload BLOB)');
fbird_query($link, "INSERT INTO blob_export_t VALUES (1, 'secret-blob-payload')");
$row = fbird_fetch_row(fbird_query($link, 'SELECT payload FROM blob_export_t WHERE id = 1'));
if (!$row || !$row[0]) { die("fixture failed: no blob id\n"); }
$blob_id = $row[0];

/* 1. php://output: exfiltration via response smuggling - must be rejected.
 *    RED anchor: on unpatched code the blob bytes are WRITTEN TO STDOUT. */
$r1 = @fbird_blob_export($link, $blob_id, 'php://output');
echo 'php://output rejected: ' . ($r1 === false ? 'yes' : 'NO - blob bytes reached the output stream') . "\n";
echo 'php://output error: ' . (strpos(fbird_errmsg(), 'URL stream wrappers') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

/* 2. ftp://: exfiltration to a remote host - must be rejected */
$r2 = @fbird_blob_export($link, $blob_id, 'ftp://127.0.0.1:20021/exfil.txt');
echo 'ftp:// rejected: ' . ($r2 === false ? 'yes' : 'NO') . "\n";
echo 'ftp:// error: ' . (strpos(fbird_errmsg(), 'URL stream wrappers') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

/* 3. local path: behavior unchanged (guard) */
$tmp = tempnam(sys_get_temp_dir(), 'fbird_sec_export_');
$r3 = fbird_blob_export($link, $blob_id, $tmp);
$content = $r3 ? file_get_contents($tmp) : '(export failed)';
echo 'local export ok: ' . ($r3 === true && $content === 'secret-blob-payload' ? 'yes' : 'NO: ' . fbird_errmsg()) . "\n";
@unlink($tmp);

fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
php://output rejected: yes
php://output error: ok
ftp:// rejected: yes
ftp:// error: ok
local export ok: yes
=== DONE ===

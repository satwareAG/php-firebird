--TEST--
feat: fbird_blob_export (to file)
--CREDITS--
v12.1.0 M2 (#376) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_blob_export')) die('skip gap: fbird_blob_export() not yet implemented (see #376)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $link;
$res = fbird_query($link, "SELECT rdb\$description FROM rdb\$database WHERE rdb\$description IS NOT NULL ROWS 1");
if ($res) {
    $row = fbird_fetch_row($res);
    if ($row[0]) {
        $tmpfile = tempnam(sys_get_temp_dir(), "fb_export");
        $r = fbird_blob_export($link, $row[0], $tmpfile);
        echo "OK blob_export: " . ($r ? "true" : "false") . "\n";
        @unlink($tmpfile);
    } else {
        echo "SKIP no blob to export\n";
    }
    fbird_free_result($res);
} else {
    echo "SKIP no blob column found\n";
}
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===

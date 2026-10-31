--TEST--
SEC FR-001 (#729): fbird_list_fields()/fbird_meta_data() must not interpolate caller-supplied relation names (quote-breakout + truncation fixtures reject loudly)
--CREDITS--
v13.4.0 security hardening train (#728) - FR-001 wire observer
--SKIPIF--
<?php
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
global $test_base;
$link = fbird_connect($test_base);

/* legit fixture: relation with a distinctive column */
fbird_query($link, 'CREATE TABLE safe_table (safe_col INTEGER)');
$cols = fbird_list_fields($link, 'safe_table');
echo 'legit lowercase fold: ' . (is_array($cols) && in_array('SAFE_COL', $cols, true) ? 'ok' : 'FAIL') . "\n";

/* 1. quote-breakout: a single quote must not alter SQL semantics */
$inj = @fbird_list_fields($link, "X' OR '1'='1");
echo 'breakout rejected: ' . ($inj === false ? 'yes' : 'NO - leaked ' . count((array) $inj) . ' rows') . "\n";
echo 'breakout error: ' . (strpos(fbird_errmsg(), 'Invalid table name') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

$meta = @fbird_meta_data($link, "X' OR '1'='1");
echo 'meta breakout rejected: ' . ($meta === false ? 'yes' : 'NO') . "\n";

/* 2. oversized name: truncation-based breakout (old code cuts at 255 bytes mid-literal) */
$long = str_repeat('A', 250) . "' OR '1'='1";
$r = @fbird_list_fields($link, $long);
echo 'oversized rejected: ' . ($r === false ? 'yes' : 'NO') . "\n";
echo 'oversized error: ' . (strpos(fbird_errmsg(), 'Invalid table name') !== false ? 'ok' : 'FAIL: ' . fbird_errmsg()) . "\n";

fbird_close($link);
echo "=== DONE ===\n";
?>
--EXPECT--
legit lowercase fold: ok
breakout rejected: yes
breakout error: ok
meta breakout rejected: yes
oversized rejected: yes
oversized error: ok
=== DONE ===

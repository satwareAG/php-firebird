--TEST--
Issue #671: default connect must not change the database header (forced writes)
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// Regression for #671: since 13.0.0 every default fbird_connect() sent
// isc_dpb_force_write = 0 ("switch to async writes") in the attach DPB
// (_php_fbird_connect_link hardcoded the SYNC slot to 0 and dropped the
// parsed $sync argument). Firebird 4+ requires CHANGE_HEADER_SETTINGS for
// that (non-SYSDBA connects failed); worse, for privileged users the flip
// silently persisted into the database header.
//
// Oracle: MON$FORCED_WRITES (ODS 11.1, FB 2.1+) after each connect. The
// per-test database is created with the server default (forced writes ON).
//
// - default connect          : must leave the header at 1
// - explicit sync = 0 (async): legitimately flips to 0 (engine-faithful,
//                              SYSDBA passes the privilege check)
// - explicit sync = 1 (sync) : flips back to 1 (proves $sync reaches the wire)
// - explicit sync = -1       : "not set" - must leave the header unchanged

function forced_writes($c): string {
    $q = @fbird_query($c, 'SELECT MON$FORCED_WRITES FROM MON$DATABASE');
    if (!$q) { return 'ERR: ' . trim(fbird_errmsg()); }
    $r = fbird_fetch_row($q);
    fbird_free_result($q);
    return $r === false ? 'no-row' : (string)$r[0];
}

init_db();

// 1. Default connect: must NOT touch the header.
$c = fbird_connect($test_base);
echo 'default connect: ', forced_writes($c), "\n";
fbird_close($c);

// 2. Explicit sync = 0: engine-faithful flip (documented, privileged users).
// Note: empty strings, not null - the zpp "s" spec does not accept null.
$c = fbird_connect($test_base, '', '', '', 0, 0, '', 0);
echo 'explicit sync=0: ', forced_writes($c), "\n";
fbird_close($c);

// 3. Explicit sync = 1: proves the argument reaches the wire.
$c = fbird_connect($test_base, '', '', '', 0, 0, '', 1);
echo 'explicit sync=1: ', forced_writes($c), "\n";
fbird_close($c);

// 4. Explicit sync = -1: "not set", header unchanged.
$c = fbird_connect($test_base, '', '', '', 0, 0, '', -1);
echo 'explicit sync=-1: ', forced_writes($c), "\n";
fbird_close($c);
?>
--EXPECT--
default connect: 1
explicit sync=0: 0
explicit sync=1: 1
explicit sync=-1: 1

--TEST--
Issue #696: fbird_server_version() returns the clean server version string
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// Regression for #696: the parser returned the raw isc_info_firebird_version
// clump VALUE (2-byte clump length taken as string length, string start at
// buffer+3). Depending on the path the client library took, that value is:
//
//   - direct engine reply  (inf.cpp):   [count=1][len1][version string]
//   - remote merged reply  (merge.cpp): [len1][merged version string]
//
// Both contain length-prefix bytes the caller must strip, and on the remote
// path the merged string can carry multi-server/protocol fragments
// ("/tcp (host)/P15:C", unrelated Firebird 5.0 entries). The stub contract
// (stubs/firebird-stubs.php) promises the clean version string of the
// CONNECTED server, e.g. "LI-V3.0.14.33856 Firebird 3.0".
//
// Oracle: full-string match against the single clean version shape. Any
// prefix byte, binary octet, or merged fragment fails the regex.

init_db();

$c = fbird_connect($test_base);
if (!$c) {
    echo "CONNECT FAILED\n";
    exit(1);
}

$v = fbird_server_version($c);

var_dump(is_string($v));

// Exactly one clean version string, nothing before or after.
$clean = is_string($v)
    && preg_match('/^(LI|WI)-V\d+\.\d+\.\d+\.\d+ Firebird \d+\.\d+$/', $v) === 1;
var_dump($clean);

// No binary/control bytes (length prefixes show up as octets < 0x20 or > 0x7e).
$printable = is_string($v)
    && preg_match('/[\x00-\x1f\x7f-\xff]/', $v) === 0;
var_dump($printable);

// No merged protocol fragments.
$no_fragments = is_string($v)
    && strpos($v, '/tcp') === false
    && strpos($v, '/P') === false;
var_dump($no_fragments);

fbird_close($c);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)

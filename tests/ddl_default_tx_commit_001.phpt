--TEST--
Issue #572: DDL via prepare+execute on the default tx must commit immediately
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

// Connection A: DDL via prepare_ex+execute on the DEFAULT transaction
// (trans_handle = null, mirroring the doctrine driver's
// fbird_prepare_ex($conn, $sql, getActiveTransaction()=null) call)
// (no explicit transaction resource). Pre-fix (#572): the statement
// executes fine but the default tx only commits at connection close,
// so other connections cannot see the table.
$c1 = fbird_connect($test_base);
var_dump($c1 instanceof Firebird\Connection);

$q = fbird_prepare_ex($c1, 'CREATE TABLE T572_DDL (id INTEGER)', null);
$r = fbird_execute($q);
var_dump($r === true); // DDL executes return true, not a result resource
fbird_free_query($q);

// Connection B: a fresh physical connection must see the table NOW,
// while $c1 is still open. This is the customer-visible #572 symptom.
$c2 = fbird_connect($test_base, '', '', '', 0, 3, '', FBIRD_CONNECT_FORCE_NEW);
var_dump($c2 instanceof Firebird\Connection && $c2 !== $c1);

$r2 = fbird_query($c2, 'SELECT COUNT(*) FROM T572_DDL');
var_dump($r2 !== false);
if ($r2 !== false) {
    $row = fbird_fetch_row($r2);
    var_dump($row !== false && (int)$row[0] === 0);
    fbird_free_result($r2);
}

// cleanup on A
$q3 = fbird_prepare_ex($c1, 'DROP TABLE T572_DDL', null);
$r3 = fbird_execute($q3);
var_dump($r3 === true);
fbird_free_query($q3);

// jane: explicit close dodges the pre-existing FORCE_NEW shutdown segfault
// (#580 - two live links corrupt IAttachment at request shutdown); tracked there
fbird_close($c2);
unset($c2);

echo "done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
done
--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

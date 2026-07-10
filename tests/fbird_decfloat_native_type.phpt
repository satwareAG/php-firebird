--TEST--
Firebird 4.0+ DECFLOAT native type: bind, fetch, OOP (#417)
--SKIPIF--
<?php
require_once 'skipif.inc';
/* jane: PHP 8.2 crashes in method dispatch for DecFloat objects (internal hack).
 * PHP 8.3+ works. Issue tracked separately. */
if (PHP_VERSION_ID < 80300) die('skip DecFloat native type requires PHP 8.3+');
require_once __DIR__ . '/fb_version_probe.inc';
if (!fb_server_supports('DECFLOAT')) die('skip DECFLOAT not supported (requires FB4+)');
?>
--FILE--
<?php
require_once __DIR__ . '/firebird.inc';

$db = fbird_connect($test_base, $user, $password);
if (!$db) die("Connection failed\n");

$tr = fbird_trans($db);

/* Create table with DECFLOAT columns */
fbird_query($tr, "RECREATE TABLE decfloat_test (
    id INTEGER PRIMARY KEY,
    df16 DECFLOAT(16),
    df34 DECFLOAT(34)
)");
fbird_commit_ret($tr);

echo "=== Test 1: Insert via parameterized query (bind path) ===\n";
fbird_query($tr, "INSERT INTO decfloat_test (id, df16, df34) VALUES (?, ?, ?)",
    1, "3.141592653589793", "3.141592653589793238462643383279503");
fbird_commit($tr);

echo "Insert: ok\n";

echo "\n=== Test 2: Fetch returns Firebird\\DecFloat objects (procedural) ===\n";
$tr = fbird_trans($db);
$res = fbird_query($tr, "SELECT df16, df34 FROM decfloat_test WHERE id = 1");
$row = fbird_fetch_object($res);
fbird_free_result($res);
fbird_commit($tr);

echo "df16 class: " . get_class($row->DF16) . "\n";
echo "df16 value: " . $row->DF16->__toString() . "\n";
echo "df16 precision: " . $row->DF16->toPrecision() . "\n";
echo "df16 rawBytes len: " . strlen($row->DF16->rawBytes()) . "\n";

echo "df34 class: " . get_class($row->DF34) . "\n";
echo "df34 value: " . $row->DF34->__toString() . "\n";
echo "df34 precision: " . $row->DF34->toPrecision() . "\n";
echo "df34 rawBytes len: " . strlen($row->DF34->rawBytes()) . "\n";

echo "\n=== Test 3: PDO fetch returns Firebird\\DecFloat objects ===\n";
$db_res = $db;  /* Save before pdo_fbird.inc redefines $db */
if (!extension_loaded('pdo_fbird')) {
    echo "pdo_fbird not loaded, skipping\n";
} else {
require_once __DIR__ . '/pdo_fbird.inc';
$pdo = pdo_fbird_connect();
$pdo->exec("RECREATE TABLE decfloat_test (
    id INTEGER PRIMARY KEY,
    df16 DECFLOAT(16),
    df34 DECFLOAT(34)
)");
$pdo->exec("INSERT INTO decfloat_test (id, df16, df34) VALUES (1, 3.141592653589793, 3.141592653589793238462643383279503)");
$stmt = $pdo->query("SELECT df16, df34 FROM decfloat_test WHERE id = 1");
$row = $stmt->fetch(PDO::FETCH_ASSOC);
$stmt->closeCursor();

echo "pdo df16 class: " . get_class($row['DF16']) . "\n";
echo "pdo df16 value: " . $row['DF16']->__toString() . "\n";
echo "pdo df16 precision: " . $row['DF16']->toPrecision() . "\n";

echo "pdo df34 class: " . get_class($row['DF34']) . "\n";
echo "pdo df34 value: " . $row['DF34']->__toString() . "\n";
echo "pdo df34 precision: " . $row['DF34']->toPrecision() . "\n";
}

echo "\n=== Test 4: Firebird\\DecFloat constructor ===\n";
$d = new Firebird\DecFloat("123.456");
echo "construct value: " . $d->__toString() . "\n";
echo "construct precision: " . $d->toPrecision() . "\n";

$d2 = Firebird\DecFloat::fromString("0.0001");
echo "fromString value: " . $d2->__toString() . "\n";
echo "fromString precision: " . $d2->toPrecision() . "\n";

echo "\n=== Test 5: Special values (Inf, NaN) ===\n";
$tr = fbird_trans($db_res);
fbird_query($tr, "INSERT INTO decfloat_test (id, df16, df34) VALUES (?, ?, ?)",
    2, "INF", "-INF");
fbird_commit($tr);

$tr = fbird_trans($db_res);
$res = fbird_query($tr, "SELECT df16, df34 FROM decfloat_test WHERE id = 2");
$row = fbird_fetch_object($res);
fbird_free_result($res);
fbird_commit($tr);

echo "inf df16: " . $row->DF16->__toString() . "\n";
echo "inf df34: " . $row->DF34->__toString() . "\n";

/* Cleanup */
$tr = fbird_trans($db_res);
fbird_query($tr, "DROP TABLE decfloat_test");
fbird_commit($tr);
fbird_close($db_res);
if (isset($pdo)) unset($pdo);

echo "\nDone.\n";
?>
--EXPECTF--
=== Test 1: Insert via parameterized query (bind path) ===
Insert: ok

=== Test 2: Fetch returns Firebird\DecFloat objects (procedural) ===
df16 class: Firebird\DecFloat
df16 value: 3.141592653589793
df16 precision: 16
df16 rawBytes len: 8
df34 class: Firebird\DecFloat
df34 value: 3.141592653589793238462643383279503
df34 precision: 34
df34 rawBytes len: 16

=== Test 3: PDO fetch returns Firebird\DecFloat objects ===
%a
=== Test 4: Firebird\DecFloat constructor ===
construct value: 123.456
construct precision: 34
fromString value: 0.0001
fromString precision: 34

=== Test 5: Special values (Inf, NaN) ===
inf df16: %s
inf df34: -Infinity

Done.

--CLEAN--
<?php require_once __DIR__ . '/clean.inc'; ?>

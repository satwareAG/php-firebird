--TEST--
feat: fbird_escape_literal / escape_identifier
--CREDITS--
v12.1.0 M2 (#374) - procedural parity RED test
--SKIPIF--
<?php
if (!function_exists('fbird_escape_literal')) die('skip gap: fbird_escape_literal() not yet implemented (see #374)');
require_once __DIR__ . '/../firebird.inc';
?>
--FILE--
<?php
require_once __DIR__ . '/../firebird.inc';
$lit = fbird_escape_literal("O'Brien");
echo "OK escape_literal: $lit\n";
$id = fbird_escape_identifier("My Table");
echo "OK escape_identifier: $id\n";
echo "=== DONE ===\n";
?>
--EXPECT--
=== DONE ===

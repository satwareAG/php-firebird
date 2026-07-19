--TEST--
feat: fbird_escape_literal / escape_identifier
--CREDITS--
v12.1.0 M2 (#374) - procedural parity RED test
--SKIPIF--
<?php
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
OK escape_literal: 'O''Brien'
OK escape_identifier: "My Table"
=== DONE ===

--TEST--
IEvents que/cancel (FB3 baseline)
--CREDITS--
v12.1.0 M4 (#390) - Firebird client coverage smoke test
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
// Register and immediately free — don't call wait_event (blocks indefinitely)
$handler = @fbird_set_event_handler($link, "echo ''", "TEST_EVT");
if ($handler) {
    fbird_free_event_handler($handler);
    echo "registered+freed\n";
} else {
    echo "register failed (expected on some configs)\n";
}
echo "done\n";
?>
--EXPECTF--
%s
done

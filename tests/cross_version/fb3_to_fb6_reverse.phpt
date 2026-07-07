--TEST--
REVERSE: FB3 client to FB6 server (document expected auth failure)
--CREDITS--
v12.1.0 M5 (#408) - deferred to M8 (no FB6 Docker image available)
--SKIPIF--
<?php die('skip FB 6.0 server not available - deferred to M8 stretch milestone'); ?>
--FILE--
<?php echo "This test always skips.\n"; ?>
--EXPECT--
This test always skips.

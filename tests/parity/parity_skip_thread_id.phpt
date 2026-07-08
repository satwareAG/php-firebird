--TEST--
skip: fbird_thread_id / fbird_thread_safe (FB protocol N/A)
--CREDITS--
v12.1.0 M2 (#384) - legitimate non-gap documentation
--SKIPIF--
<?php die('skip Firebird protocol is connection-based, not thread-based. thread_id is not applicable.'); ?>
--FILE--
<?php echo "This test always skips - documenting non-gap.\n"; ?>
--EXPECT--
This test always skips - documenting non-gap.

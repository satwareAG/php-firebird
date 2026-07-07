--TEST--
skip: fbird_warning_count / fbird_get_warnings (engine has no warning channel)
--CREDITS--
v12.1.0 M2 (#382) - legitimate non-gap documentation
--SKIPIF--
<?php die('skip Firebird has no warning stream (unlike MySQL). This is an engine limitation, not a driver gap.'); ?>
--FILE--
<?php echo "This test always skips - documenting non-gap.\n"; ?>
--EXPECT--
This test always skips - documenting non-gap.

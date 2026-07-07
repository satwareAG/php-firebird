--TEST--
skip: fbird_select_db (Firebird N/A - DB bound to attachment)
--CREDITS--
v12.1.0 M2 (#383) - legitimate non-gap documentation
--SKIPIF--
<?php die('skip In Firebird, database is bound to attachment (connection). select_db is not applicable.'); ?>
--FILE--
<?php echo "This test always skips - documenting non-gap.\n"; ?>
--EXPECT--
This test always skips - documenting non-gap.

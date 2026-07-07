--TEST--
FB5 client to FB3 server
--CREDITS--
v12.1.0 M5 (#404) - cross-version compatibility test
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
if (!is_fb_server_available("3.0")) die('skip FB 3.0 server not available');
?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
$conn = cross_version_connect("3.0");
fbird_query($conn, "SELECT 1 FROM rdb\$database");
fbird_close($conn);
echo "done\n";
?>
--EXPECT--
done

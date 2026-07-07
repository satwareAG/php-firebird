--TEST--
FB5 client to FB 2.5 server backward compat
--CREDITS--
v12.1.0 M5 (#405) - cross-version compatibility test
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
if (!is_fb_server_available("2.5")) die('skip FB 2.5 server not available');
?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
$conn = cross_version_connect("2.5");
fbird_query($conn, "SELECT 1 FROM rdb\$database");
$res = fbird_query($conn, "SELECT rdb\$relation_name FROM rdb\$relations WHERE rdb\$system_flag = 0 ROWS 1");
fbird_free_result($res);
fbird_close($conn);
echo "done\n";
?>
--EXPECT--
done

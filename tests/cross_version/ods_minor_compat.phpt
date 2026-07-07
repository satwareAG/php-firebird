--TEST--
ODS minor-version compatibility (FB4+)
--CREDITS--
v12.1.0 M5 (#409) - cross-version compatibility test
--SKIPIF--
<?php
require_once __DIR__ . '/skipif.inc';
if (!is_fb_server_available("4.0")) die('skip FB 4.0 server not available');
?>
--FILE--
<?php
require_once __DIR__ . '/skipif.inc';
$conn = cross_version_connect("4.0");
$res = fbird_query($conn, "SELECT rdb\$get_context(\x27SYSTEM\x27, \x27ENGINE_VERSION\x27) FROM rdb\$database");
fbird_fetch_row($res);
fbird_free_result($res);
$info = fbird_connection_info($conn);
fbird_close($conn);
echo "done\n";
?>
--EXPECT--
done

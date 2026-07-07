--TEST--
Get fbclient version
--SKIPIF--
<?php
include("skipif.inc");
skip_if_ext_lt(10);
?>
--FILE--
<?php

// Issue #308: verify return type is float/double, not string
var_dump(is_float(fbird_get_client_version()));

var_dump(
    fbird_get_client_version() === (float)fbird_get_client_major_version() + fbird_get_client_minor_version() / 10
);

?>
--EXPECT--
bool(true)
bool(true)

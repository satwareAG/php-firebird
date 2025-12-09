--TEST--
Get fbclient version
--SKIPIF--
<?php
include("skipif.inc");
skip_if_ext_lt(61);
?>
--FILE--
<?php

var_dump(
    fbird_get_client_version() === (float)fbird_get_client_major_version() + fbird_get_client_minor_version() / 10
);

?>
--EXPECT--
bool(true)

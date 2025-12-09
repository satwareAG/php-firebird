<?php

if (!extension_loaded("firebird")) print "skip firebird extension not available";
require("firebird.inc");
if(!@ibase_connect($test_base)){
        die("skip: cannot connect");
}

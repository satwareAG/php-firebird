--TEST--
Compare var_export behavior with standard resources
--FILE--
<?php
$f = fopen(__FILE__, 'r');
echo "File dump: "; var_dump($f);
echo "File export: "; var_export($f);
echo "\n";
fclose($f);
?>
--EXPECTF--
File dump: resource(%d) of type (stream)
File export: NULL

--TEST--
Build-identity channel (#688-P1 FR-001): FBIRD_BUILD_IDENTITY carries interpreter + API + extension version
--SKIPIF--
<?php require_once __DIR__ . '/skipif.inc'; ?>
--FILE--
<?php
$id = FBIRD_BUILD_IDENTITY;
echo "identity=", $id, "\n";
printf("php_matches=%s\n", (strpos($id, 'php=' . PHP_VERSION_ID) !== false) ? 'yes' : 'no');
printf("api_prefix=%s\n", (strpos($id, 'api=') !== false) ? 'present' : 'missing');
printf("ext_matches=%s\n", (strpos($id, 'ext=' . phpversion('firebird')) !== false) ? 'yes' : 'no');
--EXPECTF--
identity=php=%d api=%d ext=%s
php_matches=yes
api_prefix=present
ext_matches=yes

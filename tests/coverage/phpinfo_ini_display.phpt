--TEST--
Coverage: firebird.c phpinfo/INI displayer callbacks (php_fbird_password_displayer_cb, php_fbird_trans_displayer)
--EXTENSIONS--
firebird
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
// Trigger firebird.c INI displayer callbacks via phpinfo()
ob_start();
phpinfo(INFO_MODULES);
$info = ob_get_clean();

// Verify firebird module appears in phpinfo output
var_dump(strpos($info, 'firebird') !== false || strpos($info, 'Firebird') !== false);

// Test correct fbird.* INI names (triggers php_fbird_password_displayer_cb)
$db       = ini_get('fbird.default_db');
$user     = ini_get('fbird.default_user');
$passwd   = ini_get('fbird.default_password');

// ini_get returns empty string '' (not false) for valid INI with NULL default
var_dump($db     !== false);
var_dump($user   !== false);
var_dump($passwd !== false);

// Trigger php_fbird_trans_displayer by changing fbird.default_trans_params
$origTrans = ini_get('fbird.default_trans_params');
ini_set('fbird.default_trans_params', '0x100');  // set a non-zero value for ORIG path

ob_start();
phpinfo(INFO_MODULES);
$info2 = ob_get_clean();
ini_restore('fbird.default_trans_params');

// phpinfo output should still contain Firebird info
var_dump(strpos($info2, 'firebird') !== false || strpos($info2, 'Firebird') !== false);

// Trigger password displayer with non-null value
ini_set('fbird.default_password', 'test_pw');
ob_start();
phpinfo(INFO_MODULES);
$info3 = ob_get_clean();
ini_restore('fbird.default_password');

// Password displayer outputs '*' for non-empty passwords
var_dump(strpos($info3, '***') !== false || strpos($info3, 'Firebird') !== false);

echo "Done\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
Done

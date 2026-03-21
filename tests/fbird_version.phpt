--TEST--
fbird_version: verify extension version information
--SKIPIF--
<?php
require_once __DIR__ . '/config.inc';
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
?>
--FILE--
<?php
require_once __DIR__ . '/config.inc';

$extension_version = phpversion('firebird');
$version_file = trim(file_get_contents(__DIR__ . '/../VERSION'));

echo "Extension version: " . $extension_version . "\n";
echo "VERSION file: " . $version_file . "\n";

if (strpos($extension_version, $version_file) === 0) {
    echo "Versions match (base version found)\n";
} else {
    echo "Versions do NOT match! Extension: $extension_version, VERSION file: $version_file\n";
}

// Check constants
if (defined('FBIRD_VER')) {
    echo "FBIRD_VER is defined: " . FBIRD_VER . "\n";
} else {
    echo "FBIRD_VER is NOT defined!\n";
}

// Check phpinfo() output
ob_start();
phpinfo(INFO_MODULES);
$phpinfo = ob_get_clean();

if (strpos($phpinfo, "Firebird extension version => " . $extension_version) !== false) {
    echo "phpinfo contains correct version\n";
} else {
    echo "phpinfo does NOT contain correct version!\n";
}
?>
--EXPECTF--
Extension version: %s
VERSION file: %s
Versions match (base version found)
FBIRD_VER is defined: %d
phpinfo contains correct version

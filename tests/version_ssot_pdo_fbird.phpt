--TEST--
Version SSOT: pdo_fbird extension reports the VERSION.txt version (issue 672)
--SKIPIF--
<?php if (!extension_loaded('pdo_fbird')) { echo 'skip pdo_fbird extension not loaded'; } ?>
--FILE--
<?php
// Pdo twin of the #672 class oracle: every Windows pdo_fbird.dll before
// the fix reported "0.0.0-unknown" because the Windows configure lane
// never defined PHP_PDO_FBIRD_VERSION at all.
$version = trim(file_get_contents(dirname(__DIR__) . '/pdo_fbird/VERSION.txt'));
if (!preg_match('/^\d+\.\d+\.\d+/', $version)) {
    die("pdo_fbird/VERSION.txt invalid: '$version'\n");
}
$actual = phpversion('pdo_fbird');
echo $actual === $version ? "SSOT OK" : "SSOT FAIL: phpversion('$actual') != VERSION.txt('$version')";
--EXPECT--
SSOT OK

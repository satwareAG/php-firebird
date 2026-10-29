--TEST--
Version SSOT: firebird extension reports the VERSION.txt version (issue 672)
--SKIPIF--
<?php if (!extension_loaded('firebird')) { echo 'skip firebird extension not loaded'; } ?>
--FILE--
<?php
// Regression oracle for the #672 class: the built extension must report
// exactly the version declared in the repo's VERSION.txt. When configure
// reads the wrong file (the #672 bug: config.w32 read '.\VERSION'), the
// DLL falls back to "0.0.0-unknown" and this test fails on every lane.
$version = trim(file_get_contents(dirname(__DIR__) . '/VERSION.txt'));
if (!preg_match('/^\d+\.\d+\.\d+/', $version)) {
    die("VERSION.txt invalid: '$version'\n");
}
$actual = phpversion('firebird');
echo $actual === $version ? "SSOT OK" : "SSOT FAIL: phpversion('$actual') != VERSION.txt('$version')";
--EXPECT--
SSOT OK

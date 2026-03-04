@echo off
@REM
@REM Must be called under phpsdk-<php_vers>-<arch>.bat
@REM
@REM Calling script should set variables: <pfb_php_vers> <pfb_php_tag>
@REM
@REM Example: pfb_php_tag=7.4.13
@REM Example: pfb_php_vers=7.4

if "%pfb_php_vers%" == "" (
    echo pfb_php_vers varible not set
    exit 1
)

if "%pfb_php_tag%" == "" (
    echo pfb_php_tag varible not set
    exit 1
)

call phpsdk_buildtree php%pfb_php_vers%
git clone --depth 1 --branch %pfb_php_tag% https://github.com/php/php-src.git
cd php-src

@REM Note: PHP 8.0 removed ext/interbase from php-src. This extension requires PHP 8.1+.
@REM PHP 7.x builds are no longer supported.

call phpsdk_deps --update

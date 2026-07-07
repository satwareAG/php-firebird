<?php
/**
 * Standard SKIPIF + setup for Firebird client coverage tests.
 *
 * Unlike firebird.inc (which creates a new DB), this helper connects to
 * an existing test DB directly. Sets up $conn, $test_base, $user, $password.
 *
 * @package php-firebird
 * @since   12.1.0
 */

declare(strict_types=1);

if (!extension_loaded('firebird')) {
    die('skip firebird extension not loaded');
}

$db = getenv('FIREBIRD_DB_PATH') ?: '/firebird/data/test.fdb';
$host = getenv('FIREBIRD_HOST') ?: 'localhost';
$GLOBALS['user'] = 'SYSDBA';
$GLOBALS['password'] = 'masterkey';
$GLOBALS['test_base'] = "{$host}/3050:{$db}";

$conn = @fbird_connect($GLOBALS['test_base'], $GLOBALS['user'], $GLOBALS['password']);

if (!$conn) {
    die('skip cannot connect to test DB: ' . fbird_errmsg());
}

$GLOBALS['link'] = $conn;
$GLOBALS['conn'] = $conn;

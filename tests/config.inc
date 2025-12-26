<?php

$user = 'SYSDBA';
$password = 'masterkey';
$hostEnv = getenv('FIREBIRD_HOST');
// Default to "localhost" for CI environments where Firebird listens on TCP only.
// The empty string would attempt embedded/local connection which may not work
// in server-only installations (Classic/SuperClassic without embedded support).
$host = ($hostEnv !== false && $hostEnv !== '') ? $hostEnv : "localhost";
ini_set('fbird.default_user',$user);
ini_set('fbird.default_password',$password);

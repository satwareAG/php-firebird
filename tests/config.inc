<?php

$user = 'SYSDBA';
$password = 'masterkey';
$hostEnv = getenv('FIREBIRD_HOST');
$host = ($hostEnv !== false) ? $hostEnv : "";
ini_set('fbird.default_user',$user);
ini_set('fbird.default_password',$password);

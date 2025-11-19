<?php

$user = 'SYSDBA';
$password = 'masterkey';
$hostEnv = getenv('FIREBIRD_HOST');
$host = ($hostEnv !== false) ? $hostEnv : "";
ini_set('ibase.default_user',$user);
ini_set('ibase.default_password',$password);

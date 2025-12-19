<?php
// satware-docs/benchmarks/config.php
// Shared configuration for Firebird performance benchmarks.
//
// Adjust the defaults below to match a local Firebird instance if you
// want a baseline. For Docker-based testing, you can override all values
// via environment variables:
//   FB_DSN, FB_HOST, FB_USER, FB_PASSWORD, FB_CHARSET
//
// Examples for Docker setup in satware-docs/docker-compose.yml:
//   Firebird 2.5: localhost/3050:/firebird/data/test.fdb
//   Firebird 3.0: localhost/3051:/firebird/data/test.fdb
//   Firebird 4.0: localhost/3052:/firebird/data/test.fdb
//   Firebird 5.0: localhost/3053:/firebird/data/test.fdb

$default = [
    // Example DSN (adjust as needed):
    //   firebird:dbname=host/port:/path/to/database.fdb;charset=UTF8
    'dsn'      => 'firebird:dbname=localhost:/path/to/test.fdb;charset=UTF8',

    // Classic ibase extension uses host/path in this form.
    'host'     => 'localhost:/path/to/test.fdb',

    'user'     => 'SYSDBA',
    'password' => 'masterkey',
    'charset'  => 'UTF8',
];

return [
    'dsn'      => getenv('FB_DSN')      !== false ? getenv('FB_DSN')      : $default['dsn'],
    'host'     => getenv('FB_HOST')     !== false ? getenv('FB_HOST')     : $default['host'],
    'user'     => getenv('FB_USER')     !== false ? getenv('FB_USER')     : $default['user'],
    'password' => getenv('FB_PASSWORD') !== false ? getenv('FB_PASSWORD') : $default['password'],
    'charset'  => getenv('FB_CHARSET')  !== false ? getenv('FB_CHARSET')  : $default['charset'],
];

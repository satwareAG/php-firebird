# PHP Firebird Extension Stubs

[![PHP Version](https://img.shields.io/badge/php-%3E%3D8.1-8892BF.svg)](https://php.net/)
[![License](https://img.shields.io/badge/license-PHP--3.01-blue.svg)](LICENSE)

IDE autocompletion and static analysis stubs for the [php-firebird](https://github.com/satwareAG/php-firebird) extension.

## Installation

```bash
composer require --dev satwareag/php-firebird-stubs
```

## Purpose

This package provides stub files that enable:

- **IDE Autocompletion**: Full function signatures, parameter hints, and return types for PhpStorm, VSCode (Intelephense), and other IDEs
- **Static Analysis**: PHPStan and Psalm support for analyzing code that uses the php-firebird extension
- **Documentation**: Inline PHPDoc comments describing each function and constant

## What's Included

- `firebird-stubs.php` - Constants and function declarations for all `fbird_*` functions
- `firebird-classes.php` - Extension-provided classes (`Firebird\Event`, `Firebird\Exception`)

## Usage

### PHPStan

The stubs are automatically loaded via Composer's autoload. If you need to reference them explicitly in your `phpstan.neon`:

```neon
parameters:
    scanFiles:
        - vendor/satwareag/php-firebird-stubs/firebird-stubs.php
        - vendor/satwareag/php-firebird-stubs/firebird-classes.php
```

### Psalm

Add to your `psalm.xml`:

```xml
<stubs>
    <file name="vendor/satwareag/php-firebird-stubs/firebird-stubs.php"/>
    <file name="vendor/satwareag/php-firebird-stubs/firebird-classes.php"/>
</stubs>
```

### IDE Configuration

Most IDEs will automatically pick up the stubs via Composer autoloading. No additional configuration is typically required.

## Included Functions (v7.0.0)

All `fbird_*` functions are stubbed, including v7.0.0 additions:

| Function | Description |
|----------|-------------|
| `fbird_query_params_tx($link, $trans, $sql, ?$params)` | Explicit link + transaction + params (Doctrine DBAL) |
| `fbird_execute_statement($stmt)` | Execute a prepared statement resource |
| `fbird_execute_query($link, $sql, ...$params)` | Execute SQL with optional params |
| `fbird_execute_auto($link, $sql, ...$params)` | Auto-commit execution helper |
| `fbird_set_exception_mode(int $mode)` | Set SILENT/THROW error mode |
| `fbird_get_exception_mode()` | Get current error mode |
| `fbird_trans_start($link, array $options)` | Full TPB transaction builder |
| `fbird_savepoint($trans, string $name)` | Create named savepoint |
| `fbird_rollback_savepoint($trans, string $name)` | Rollback to savepoint |
| `fbird_release_savepoint($trans, string $name)` | Release savepoint |
| `fbird_connection_info($link)` | Connection metadata |
| `fbird_trans_info($trans)` | Transaction state info |
| `fbird_blob_seek($blob, int $offset, int $whence)` | Seekable BLOB access |
| `fbird_batch_create($stmt, $trans)` | Create batch (FB 4.0+) |
| `fbird_batch_add($batch, ...$params)` | Add row to batch |
| `fbird_batch_execute($batch)` | Execute batch |
| `fbird_batch_cancel($batch)` | Cancel batch |
| `fbird_escape_string(string $str)` | Escape string for SQL |

All classic `fbird_connect`, `fbird_query`, `fbird_prepare`, `fbird_execute`,
`fbird_fetch_*`, `fbird_trans`, `fbird_commit`, `fbird_rollback`, `fbird_blob_*`,
`fbird_service_*`, and event functions are also stubbed.

## Version Compatibility

| Stubs Version | Extension Version | PHP Version |
|---------------|-------------------|-------------|
| 7.0.x         | 7.0.x             | ≥8.1        |

## Related Packages

- [php-firebird](https://github.com/satwareAG/php-firebird) - The actual C extension providing Firebird database connectivity

## License

PHP License 3.01 - Same as PHP itself.

## Author

satware AG - [https://satware.com](https://satware.com)

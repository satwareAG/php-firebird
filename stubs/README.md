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

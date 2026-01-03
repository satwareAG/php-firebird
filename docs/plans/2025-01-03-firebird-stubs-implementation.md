# Firebird Stubs Implementation Plan

**Date**: 2025-01-03
**Status**: Approved for Implementation
**Related**: Issue #55 (PHPStan SIGSEGV fix), Research doc: `docs/research/php-extension-stub-distribution.md`

## Objective

Create and publish `satwareag/firebird-stubs` Composer package for:
- PHPStan/Psalm static analysis
- VSCode Intelephense code completion
- PhpStorm/IntelliJ IDE support

## Implementation Checklist

### Phase 1: Prepare Stubs in Main Repository

- [ ] **Enhance existing stubs** (`phpstan/fbird.stub.php`)
  - [ ] Add `@since 7.0.0` annotations to all functions
  - [ ] Add `@throws \Firebird\Exception` where applicable
  - [ ] Add `@see` cross-references between related functions
  - [ ] Add `@link` to Firebird documentation where helpful
  - [ ] Add `@deprecated` for `ibase_*` aliases (if included)

- [ ] **Create consolidated stub file** (`stubs/firebird-stubs.php`)
  - [ ] File header with package metadata
  - [ ] `if (false) { }` wrapper to prevent execution
  - [ ] Constants section with values and docs
  - [ ] Functions section with full signatures
  - [ ] Namespace classes (`Firebird\Exception`, `Firebird\Event`)

- [ ] **Validate stub syntax**
  - [ ] Run `php -l stubs/firebird-stubs.php`
  - [ ] Test with PHPStan on sample project
  - [ ] Test with Intelephense in VSCode

### Phase 2: Create Stubs Repository

- [ ] **Create GitHub repository** `satwareAG/firebird-stubs`
  - [ ] Initialize with MIT or PHP-3.01 license
  - [ ] Add README.md with installation instructions
  - [ ] Copy consolidated stub file

- [ ] **Configure repository**
  - [ ] Create `composer.json`
  - [ ] Add `.gitignore`
  - [ ] Add `.editorconfig`
  - [ ] Create GitHub Actions workflow for validation

### Phase 3: Publish to Packagist

- [ ] **Register on Packagist**
  - [ ] Submit `satwareag/firebird-stubs` package
  - [ ] Configure Packagist webhook for auto-updates

- [ ] **Create initial release**
  - [ ] Tag `v7.0.0` to match extension version
  - [ ] Write release notes

### Phase 4: Integration and Documentation

- [ ] **Update main extension README**
  - [ ] Add "IDE and Static Analysis Support" section
  - [ ] Document installation via Composer
  - [ ] Document manual configuration options

- [ ] **Test integration**
  - [ ] Test `composer require --dev satwareag/firebird-stubs`
  - [ ] Verify PHPStan auto-discovery
  - [ ] Verify Intelephense code completion
  - [ ] Verify PhpStorm code completion

---

## File Templates

### stubs/firebird-stubs.php (Consolidated)

```php
<?php
/**
 * PHP Firebird Extension Stubs
 *
 * Provides function signatures and constant definitions for static analysis
 * tools (PHPStan, Psalm, Phan) and IDE code completion (PhpStorm, VSCode).
 *
 * @package   satwareag/firebird-stubs
 * @version   7.0.0
 * @author    satware AG <info@satware.com>
 * @copyright 2025 satware AG
 * @license   PHP-3.01
 * @link      https://github.com/satwareAG/php-firebird
 * @link      https://github.com/satwareAG/firebird-stubs
 *
 * INSTALLATION:
 *   composer require --dev satwareag/firebird-stubs
 *
 * This file is for static analysis only. The actual implementation is
 * provided by the php-firebird C extension.
 */

// Prevent accidental execution
if (false) {

// =============================================================================
// CONSTANTS
// =============================================================================

/**
 * Default flags (no special options)
 * @since 7.0.0
 */
const FBIRD_DEFAULT = 0;

// ... (all constants from fbird.stub.php)

// =============================================================================
// CONNECTION FUNCTIONS
// =============================================================================

/**
 * Connect to a Firebird database.
 *
 * @param string      $database Connection string (e.g., "localhost:/path/db.fdb")
 * @param string|null $username Username (default: SYSDBA)
 * @param string|null $password Password
 * @param string|null $charset  Character set (e.g., "UTF8")
 * @param int         $buffers  Database cache buffers
 * @param int         $dialect  SQL dialect (1, 2, or 3)
 * @param string|null $role     SQL role name
 * @param int         $flags    Connection flags
 *
 * @return resource|false Connection resource or false on failure
 *
 * @throws \Firebird\Exception When exception mode is enabled
 *
 * @since 7.0.0
 * @see   fbird_pconnect() For persistent connections
 * @see   fbird_close() To close the connection
 */
function fbird_connect(
    string $database,
    ?string $username = null,
    ?string $password = null,
    ?string $charset = null,
    int $buffers = 0,
    int $dialect = 3,
    ?string $role = null,
    int $flags = 0
): mixed {}

// ... (all functions)

// =============================================================================
// CLASSES
// =============================================================================

namespace Firebird;

/**
 * Event class for database event handling.
 *
 * @since 7.0.0
 */
final class Event {}

/**
 * Exception class for Firebird errors.
 *
 * Thrown when exception mode is enabled via fbird_set_exception_mode().
 *
 * @since 7.0.0
 */
class Exception extends \Exception
{
    /**
     * Get the SQLSTATE error code.
     *
     * @return string 5-character SQLSTATE code (e.g., "42000")
     * @since 7.0.0
     */
    public function getSqlState(): string {}
}

} // end if(false)
```

### firebird-stubs/composer.json

```json
{
    "name": "satwareag/firebird-stubs",
    "description": "PHP Firebird/InterBase extension stubs for static analysis and IDE support",
    "type": "library",
    "license": "PHP-3.01",
    "keywords": [
        "firebird",
        "interbase",
        "fbird",
        "stubs",
        "phpstan",
        "psalm",
        "ide",
        "static-analysis",
        "autocomplete"
    ],
    "homepage": "https://github.com/satwareAG/firebird-stubs",
    "authors": [
        {
            "name": "satware AG",
            "email": "info@satware.com",
            "homepage": "https://satware.com"
        }
    ],
    "support": {
        "issues": "https://github.com/satwareAG/firebird-stubs/issues",
        "source": "https://github.com/satwareAG/firebird-stubs"
    },
    "require": {
        "php": ">=8.1"
    },
    "conflict": {
        "ext-interbase": "*"
    },
    "autoload": {
        "files": [
            "firebird-stubs.php"
        ]
    },
    "extra": {
        "branch-alias": {
            "dev-main": "7.0.x-dev"
        }
    },
    "config": {
        "sort-packages": true
    }
}
```

### firebird-stubs/README.md

```markdown
# Firebird Stubs

PHP stubs for the [php-firebird](https://github.com/satwareAG/php-firebird) extension.

Provides function signatures and type information for:
- **PHPStan** / **Psalm** / **Phan** static analysis
- **PhpStorm** / **IntelliJ IDEA** code completion
- **VSCode** with **Intelephense** extension

## Installation

```bash
composer require --dev satwareag/firebird-stubs
```

## Usage

### PHPStan

Stubs are auto-discovered from composer autoload. No configuration needed.

### Psalm

Stubs are loaded via composer autoload. No configuration needed.

### PhpStorm / IntelliJ IDEA

Stubs are auto-indexed from `vendor/`. No configuration needed.

### VSCode with Intelephense

Stubs are auto-indexed from `vendor/`. No configuration needed.

## Compatibility

| Stubs Version | Extension Version | PHP Version |
|---------------|-------------------|-------------|
| 7.0.x         | 7.0.x             | 8.1+        |

## Related

- [php-firebird](https://github.com/satwareAG/php-firebird) - The PHP Firebird extension
- [Firebird SQL](https://firebirdsql.org/) - The Firebird database

## License

PHP License 3.01 (same as the php-firebird extension)
```

### firebird-stubs/.github/workflows/validate.yml

```yaml
name: Validate Stubs

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  validate:
    runs-on: ubuntu-latest

    strategy:
      matrix:
        php: ['8.1', '8.2', '8.3', '8.4']

    steps:
      - uses: actions/checkout@v4

      - name: Setup PHP
        uses: shivammathur/setup-php@v2
        with:
          php-version: ${{ matrix.php }}
          tools: composer

      - name: Validate composer.json
        run: composer validate --strict

      - name: Check PHP syntax
        run: php -l firebird-stubs.php

      - name: Install PHPStan
        run: composer require --dev phpstan/phpstan

      - name: Test stubs with PHPStan
        run: |
          echo '<?php
          $conn = fbird_connect("localhost:/test.fdb");
          if ($conn !== false) {
              $result = fbird_query($conn, "SELECT 1 FROM RDB\$DATABASE");
              fbird_close($conn);
          }' > test.php
          vendor/bin/phpstan analyse test.php --level=8
```

---

## Timeline

| Phase | Duration | Target Date |
|-------|----------|-------------|
| Phase 1: Enhance stubs | 1 day | 2025-01-03 |
| Phase 2: Create repository | 1 day | 2025-01-04 |
| Phase 3: Publish to Packagist | 1 day | 2025-01-05 |
| Phase 4: Documentation | 1 day | 2025-01-06 |

## Success Criteria

1. ✅ `composer require --dev satwareag/firebird-stubs` works
2. ✅ PHPStan analyzes code using `fbird_*` functions without errors
3. ✅ VSCode Intelephense provides code completion for all functions
4. ✅ PhpStorm provides code completion and parameter hints
5. ✅ Package has >100 installs within first month

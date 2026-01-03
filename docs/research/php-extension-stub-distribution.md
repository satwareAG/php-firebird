# PHP Extension Stub Distribution Research

**Date**: 2025-01-03
**Author**: AI Assistant (for satware AG)
**Context**: GitHub Issue #55 follow-up - Providing stubs for static analysis and IDE code completion

## Executive Summary

This document analyzes best practices for distributing PHP extension stubs to enable:
- Static analysis (PHPStan, Psalm, Phan)
- IDE code completion (PhpStorm/IntelliJ, VSCode with Intelephense)
- Type checking for code using the Firebird extension

**Recommendation**: Create a **separate Composer package** `satwareag/firebird-stubs` (or contribute to `php-stubs` organization) while keeping development stubs in the main repository.

---

## 1. Research Findings

### 1.1 JetBrains phpstorm-stubs Pattern

The [JetBrains/phpstorm-stubs](https://github.com/JetBrains/phpstorm-stubs) repository is the authoritative source for PHP extension stubs used by PhpStorm and other tools.

**Key characteristics**:
- One directory per extension (e.g., `pdo/`, `mysqli/`, `redis/`)
- Function signatures with full PHPDoc annotations (`@param`, `@return`, `@since`)
- Empty function bodies (stubs don't contain implementation)
- Includes constants, classes, interfaces, and functions
- Used by PhpStorm's built-in type inference

**Contribution path**: Fork → Add stubs → Submit PR for official inclusion.

### 1.2 php-stubs Organization Pattern

The [php-stubs](https://github.com/php-stubs) GitHub organization maintains **separate repositories** for individual stub packages.

**Key characteristics**:
- One repository per extension/framework (e.g., `wordpress-stubs`, `woocommerce-stubs`)
- Single primary PHP file containing all stubs (e.g., `wordpress-stubs.php`)
- Composer package with `type: php-stubs` or `type: library`
- PHPStan auto-discovers stubs from `vendor/php-stubs/` directory
- Independent versioning matching upstream source versions

**Package structure**:
```
php-stubs/example-stubs/
├── composer.json          # Packagist metadata
├── example-stubs.php      # All stub declarations
├── LICENSE                # Usually MIT
└── README.md              # Usage instructions
```

**Example composer.json**:
```json
{
  "name": "php-stubs/example-stubs",
  "description": "Example extension stubs for static analysis",
  "type": "library",
  "license": "MIT",
  "require": {
    "php": ">=8.1"
  },
  "autoload": {
    "files": ["example-stubs.php"]
  }
}
```

### 1.3 IDE Integration Methods

#### PhpStorm/IntelliJ IDEA
1. **Built-in stubs**: Settings → PHP → PHP Runtime → Sync Extensions with Interpreter
2. **Composer packages**: Auto-indexed from `vendor/`
3. **Custom stubs**: Settings → PHP → Include Path → Add stub directory
4. **phpstorm-stubs contribution**: Official inclusion for widespread adoption

#### VSCode with Intelephense
1. **Auto-indexing**: Stubs in `vendor/` are automatically indexed
2. **Explicit configuration**: Add path to `intelephense.stubs` array in `settings.json`:
   ```json
   {
     "intelephense.stubs": [
       "apache", "bcmath", "...standard stubs...",
       "/path/to/firebird-stubs"
     ]
   }
   ```
3. **Workspace stubs**: Place `.php` stub files in project root for auto-discovery

#### PHPStan
1. **Auto-discovery**: Packages in `vendor/php-stubs/` are automatically loaded
2. **Explicit configuration**: Add to `phpstan.neon`:
   ```neon
   parameters:
       stubFiles:
           - vendor/satwareag/firebird-stubs/firebird-stubs.php
   ```
3. **scanFiles**: Optional directive for symbol discovery from stubs

#### Psalm
1. **Composer autoload**: Stubs loaded via Composer autoload
2. **Explicit configuration**: `psalm.xml` stub declaration

---

## 2. Distribution Options Analysis

### Option A: Separate Repository (Recommended)

**Structure**: `satwareAG/firebird-stubs` or contribute to `php-stubs/firebird-stubs`

**Pros**:
- Clean separation of concerns
- Independent versioning
- Follows established ecosystem patterns
- Easy for users to install via `composer require --dev`
- Can be submitted to php-stubs organization for visibility
- No coupling with C extension build process

**Cons**:
- Additional repository maintenance
- Need to keep stubs in sync with extension releases
- Two releases required (extension + stubs)

**Package name options**:
1. `satwareag/firebird-stubs` - Own namespace, full control
2. `php-stubs/firebird-stubs` - Community namespace, higher visibility (requires PR)

### Option B: Same Repository, Separate Package

**Structure**: Stubs in main repo published as separate Composer package

**Pros**:
- Single repository maintenance
- Automatic sync with extension development
- Stubs co-located with source for easy updates

**Cons**:
- Complex Composer configuration
- User confusion about extension vs stubs package
- Harder to version independently

### Option C: Bundled in Extension Repository (Current State)

**Structure**: Current `phpstan/` directory

**Pros**:
- Simple maintenance
- Already working for PHPStan

**Cons**:
- Not easily consumable by other users
- No Composer package for distribution
- IDE users must manually configure paths

---

## 3. Recommended Implementation Plan

### Phase 1: Enhance Current Stubs (Immediate)

1. **Consolidate stub files** into `stubs/firebird-stubs.php`:
   - Merge `phpstan/fbird.stub.php` and `phpstan/firebird-event.stub.php`
   - Add `@since` annotations for Firebird version compatibility
   - Add `@deprecated` annotations for legacy `ibase_*` aliases

2. **Update existing stubs with enhanced PHPDoc**:
   - Add `@see` links to official documentation
   - Include example usage in function docblocks
   - Document parameter constraints and return value meanings

### Phase 2: Create Separate Package (Short-term)

1. **Create new repository**: `satwareAG/firebird-stubs`

2. **Package structure**:
   ```
   satwareAG/firebird-stubs/
   ├── composer.json
   ├── firebird-stubs.php          # All fbird_* functions
   ├── firebird-classes.php        # Firebird\* namespace classes
   ├── firebird-constants.php      # Constants (can be combined)
   ├── LICENSE                     # PHP License 3.01
   ├── README.md                   # Usage instructions
   └── .github/
       └── workflows/
           └── validate.yml        # CI to validate stub syntax
   ```

3. **Composer.json for stubs package**:
   ```json
   {
     "name": "satwareag/firebird-stubs",
     "description": "PHP Firebird/InterBase extension stubs for static analysis and IDE support",
     "type": "library",
     "license": "PHP-3.01",
     "keywords": ["firebird", "interbase", "stubs", "phpstan", "ide", "static-analysis"],
     "authors": [
       {
         "name": "satware AG",
         "email": "info@satware.com"
       }
     ],
     "require": {
       "php": ">=8.1"
     },
     "conflict": {
       "ext-interbase": "*"
     },
     "autoload": {
       "files": ["firebird-stubs.php"]
     },
     "extra": {
       "branch-alias": {
         "dev-main": "7.0.x-dev"
       }
     }
   }
   ```

4. **Versioning strategy**:
   - Match extension major.minor version (e.g., `7.0.0` for extension v7.0.0)
   - Patch versions can differ for stub-only fixes

### Phase 3: Ecosystem Integration (Medium-term)

1. **Submit stubs to JetBrains phpstorm-stubs**:
   - Fork repository
   - Create `firebird/` directory
   - Add stubs following their format
   - Submit PR for official inclusion

2. **Consider php-stubs organization**:
   - Contact maintainers about inclusion
   - Transfer or mirror to `php-stubs/firebird-stubs`

3. **Document installation methods** in main extension README:
   ```markdown
   ## IDE and Static Analysis Support

   Install stubs for code completion and type checking:

   ```bash
   composer require --dev satwareag/firebird-stubs
   ```

   ### PHPStan
   Stubs are auto-discovered. No additional configuration needed.

   ### VSCode (Intelephense)
   Stubs are auto-indexed from vendor directory.

   ### PhpStorm
   Stubs are auto-indexed from vendor directory.
   ```

---

## 4. Stub File Format Requirements

### Required Elements

1. **Function signatures** with:
   - All parameters with types and default values
   - Return type declarations
   - PHPDoc with `@param`, `@return`, `@throws`, `@since`

2. **Constants** with values:
   ```php
   /** @since 7.0.0 */
   const FBIRD_DEFAULT = 0;
   ```

3. **Classes/Interfaces** with:
   - Property types
   - Method signatures
   - Class-level PHPDoc

### Example Stub Format

```php
<?php
/**
 * PHP Firebird Extension Stubs
 *
 * @package   php-firebird
 * @version   7.0.0
 * @author    satware AG
 * @copyright 2025 satware AG
 * @license   PHP-3.01
 * @link      https://github.com/satwareAG/php-firebird
 *
 * This file provides stub declarations for static analysis tools and IDEs.
 * It does not contain actual implementation - the real functions are provided
 * by the php-firebird C extension.
 */

// Prevent direct execution
if (false) {

/**
 * Connect to a Firebird database.
 *
 * Opens a connection to a Firebird database server. For persistent connections,
 * use {@see fbird_pconnect()} instead.
 *
 * @param string      $database Database path or connection string (e.g., "localhost:/path/to/db.fdb")
 * @param string|null $username Username for authentication (default: SYSDBA)
 * @param string|null $password Password for authentication
 * @param string|null $charset  Character set for the connection (e.g., "UTF8")
 * @param int         $buffers  Number of database cache buffers
 * @param int         $dialect  SQL dialect (1, 2, or 3)
 * @param string|null $role     SQL role name
 * @param int         $flags    Connection flags (e.g., FBIRD_CONNECT_FORCE_NEW)
 *
 * @return resource|false Connection resource on success, false on failure
 *
 * @throws \Firebird\Exception When exception mode is enabled and connection fails
 *
 * @since 7.0.0
 * @see   fbird_pconnect() For persistent connections
 * @see   fbird_close()    To close the connection
 * @link  https://firebirdsql.org/manual/isql-connect.html
 *
 * @example
 * $conn = fbird_connect('localhost:/var/db/test.fdb', 'SYSDBA', 'masterkey', 'UTF8');
 * if ($conn === false) {
 *     die('Connection failed: ' . fbird_errmsg());
 * }
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

} // end if(false)
```

---

## 5. Maintenance Workflow

### When Releasing New Extension Version

1. Update stub file with any new/changed functions
2. Update `@since` annotations for new functions
3. Tag stub package with matching version
4. Publish to Packagist

### Automation Opportunities

1. **Stub validation CI**: Parse stubs and validate syntax
2. **Signature comparison**: Compare stubs against actual extension (reflection-based)
3. **Auto-generation**: Consider using `php-stubs/generator` or custom tooling

---

## 6. References

- [JetBrains phpstorm-stubs](https://github.com/JetBrains/phpstorm-stubs)
- [php-stubs organization](https://github.com/php-stubs)
- [PHPStan Stub Files](https://phpstan.org/user-guide/stub-files)
- [Intelephense Stubs](https://github.com/bmewburn/vscode-intelephense/issues/431)
- [Psalm Stubs](https://psalm.dev/docs/running_psalm/configuration/#stubs)

---

## 7. Decision Required

**Recommendation**: Implement **Option A (Separate Repository)** with the following specifics:

| Decision | Recommendation |
|----------|----------------|
| Repository | `satwareAG/firebird-stubs` (own namespace) |
| Package name | `satwareag/firebird-stubs` |
| Versioning | Match extension versions |
| Distribution | Packagist + GitHub releases |
| phpstorm-stubs | Submit PR after package is stable |

**Next steps**:
1. Create enhanced stub file combining current stubs
2. Create new GitHub repository `satwareAG/firebird-stubs`
3. Configure Composer package and publish to Packagist
4. Update main extension README with installation instructions

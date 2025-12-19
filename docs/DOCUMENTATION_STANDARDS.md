# Documentation Standards (2025)

This document defines the documentation philosophy and standards for the php-firebird extension, following the **"Good Code Needs No Documentation"** paradigm combined with modern 2025 documentation best practices.

## Core Philosophy

### "Good Code Needs No Documentation" Paradigm

**Principle**: Code should be self-explanatory through good naming, clear structure, and logical organization. Comments should explain **WHY**, not **WHAT**.

**Five Rules for Code Comments** (TechTarget 2025):

1. **Comments explain WHY, not WHAT** - Explain the reasoning behind implementation choices
2. **Comments don't duplicate code** - Don't restate what the code already clearly says
3. **Comments clarify, not confuse** - Use clear, precise language
4. **Comments are brief** - Extensive comments indicate problematic code needing refactoring
5. **Comments explain non-obvious behavior** - Document edge cases, workarounds, and historical context

### External Documentation First

Instead of inline documentation, we prefer:

- **README.md** - Quick start and examples-first documentation
- **docs/API_REFERENCE.md** - Complete function reference
- **docs/EXAMPLES.md** - Cookbook/recipes for common tasks
- **docs/MIGRATION.md** - Migration guides between versions
- **CHANGELOG.md** - Change history following Keep a Changelog format
- **Tests as Documentation** - `.phpt` files serve as executable examples

---

## Source Code Comment Policy

### ✅ REQUIRED Comments

```c
/* 
 * Comments that explain WHY this approach was chosen:
 * - Design decisions and trade-offs
 * - Historical context (why code exists this way)
 * - Non-obvious behavior or edge cases
 * - Workarounds for external limitations
 * - Performance considerations
 * - References to issues or specifications
 */

/* Fall back to INI defaults if user/password not provided (Issue #71) */
if (ulen == 0) {
    char *ini_user = INI_STR("fbird.default_user");
    ...
}

/*
 * Firebird 3.0+ OO API Connection
 *
 * All operations use the modern OO API wrappers (fbc_*, fbt_*, etc.).
 * Note: Firebird 3.0+ is required - compile-time enforced in php_fbird_includes.h
 */
```

### ✅ PHP Extension Standard Comments

PHP extensions require specific comment formats for documentation generation:

```c
/* {{{ proto bool fbird_close([resource link_identifier])
   Close an InterBase connection */
PHP_FUNCTION(fbird_close)
{
    ...
}
/* }}} */
```

These `/* {{{ proto */` comments are **REQUIRED** for:
- Reflection API documentation
- PHP manual generation
- IDE autocompletion

### ❌ FORBIDDEN Comments

```c
// ❌ DON'T: Comments that restate the code
int count = count + 1;  // increment the counter

// ❌ DON'T: Comments for self-explanatory code
char *user = get_username();  // get the username

// ❌ DON'T: Redundant type information
int user_id;  // integer containing user ID

// ❌ DON'T: TODO comments without issue references
// TODO: fix this later

// ❌ DON'T: Commented-out code (use version control)
// old_function();
```

### Refactor Instead of Comment

**Bad** (comment explaining complex code):
```c
// Check if user is authenticated and has permission and session is valid
if (user != NULL && user->is_active && user->has_permission && session_valid(user)) {
    ...
}
```

**Good** (self-explanatory code):
```c
bool user_can_proceed = is_authenticated(user) && has_valid_session(user);
if (user_can_proceed) {
    ...
}
```

---

## External Documentation Structure

### README.md Structure (Examples-First)

```markdown
# Project Name

One-line description.

## Quick Start (30 seconds)

```php
// Minimal working example - copy-paste runnable
$db = fbird_connect('/path/to/database.fdb', 'SYSDBA', 'masterkey');
$result = fbird_query($db, "SELECT * FROM users");
while ($row = fbird_fetch_assoc($result)) {
    print_r($row);
}
fbird_close($db);
```

## Installation

## Features

## Documentation

Links to detailed docs.
```

### API Reference Structure

```markdown
# API Reference

## Connection Functions

### fbird_connect()

**Signature:**
```php
fbird_connect(
    ?string $database = null,
    ?string $username = null,
    ?string $password = null,
    ?string $charset = null,
    int $buffers = 0,
    int $dialect = 3,
    ?string $role = null,
    int $flags = 0
): resource|false
```

**Parameters:**
- `$database` - Path to database (local or remote)
- ...

**Returns:** Database link resource or `false` on failure

**Example:**
```php
$db = fbird_connect('localhost:/var/firebird/data.fdb', 'SYSDBA', 'masterkey');
```

**Notes:**
- Falls back to `fbird.default_*` INI settings when parameters are null
```

### Examples/Cookbook Structure

```markdown
# Examples & Recipes

## Basic Operations

### Connecting to a Database
### Executing Queries
### Using Transactions

## Advanced Topics

### Transaction-Aware Queries (UNIQUE FEATURE)
### Multi-Database Transactions (UNIQUE FEATURE)
### Using INI Credential Fallback

## Migration Recipes

### From ibase_* to fbird_*
```

---

## CHANGELOG Format

Follow [Keep a Changelog](https://keepachangelog.com/) format:

```markdown
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- New feature description

### Changed
- Change description

### Deprecated
- Deprecation notice

### Removed
- Removal description

### Fixed
- Bug fix description

### Security
- Security fix description

## [1.0.0] - 2025-01-15

### Added
- Initial release features
```

---

## Tests as Documentation

`.phpt` files serve as executable documentation. They should:

1. **Be self-explanatory** - Test name describes the feature
2. **Include real-world examples** - Not just edge cases
3. **Document expected behavior** - The `--EXPECT--` section is documentation

**Example:**

```php
--TEST--
fbird_connect() falls back to INI credentials when parameters omitted
--SKIPIF--
<?php include("skipif.inc"); ?>
--INI--
fbird.default_user=SYSDBA
fbird.default_password=masterkey
--FILE--
<?php
// When credentials are omitted, INI settings are used
$db = fbird_connect($database);  // Uses fbird.default_user/password
var_dump(is_resource($db));
fbird_close($db);
?>
--EXPECT--
bool(true)
```

---

## Unique Features Documentation

The php-firebird extension has unique features that **MUST** be prominently documented:

### 1. Transaction-Aware Queries

```php
// Other extensions (mysqli, pgsql): query always uses connection
mysqli_query($conn, $sql);

// php-firebird: query can use explicit transaction
$trans = fbird_trans($db);
$result = fbird_query($trans, $sql);  // Uses specific transaction
fbird_commit($trans);
```

### 2. Multi-Database Transactions

```php
// UNIQUE: Single transaction spanning multiple databases
$trans = fbird_trans(
    FBIRD_READ, $db1,   // Read access to db1
    FBIRD_WRITE, $db2   // Write access to db2
);
// Both databases participate in same transaction
```

### 3. INI Credential Fallback

```ini
; php.ini
fbird.default_user = "SYSDBA"
fbird.default_password = "masterkey"
```

```php
// Credentials from INI used when omitted
$db = fbird_connect('/path/to/db.fdb');
```

### 4. Connection Reuse with Force New Escape Hatch

```php
// Default: connection reuse (efficient)
$db1 = fbird_connect($dsn, 'user', 'pass');
$db2 = fbird_connect($dsn, 'user', 'pass');  // Returns same connection

// Force new connection when needed (matches PostgreSQL behavior)
$db3 = fbird_connect($dsn, 'user', 'pass', null, 0, 0, null, FBIRD_CONNECT_FORCE_NEW);
```

---

## Documentation Review Checklist

Before merging documentation changes:

- [ ] Examples are copy-paste runnable
- [ ] Unique features are prominently highlighted
- [ ] No redundant inline comments (explain WHY, not WHAT)
- [ ] Tests serve as executable documentation
- [ ] CHANGELOG follows Keep a Changelog format
- [ ] Parameter ordering warnings are clear (fbird_trans)
- [ ] Cross-references between docs are accurate

---

---

## Source File License Headers (SPDX)

### Datensparsamkeit Principle (Data Minimization)

**The Problem**: Legacy PHP extension headers are 24 lines per file, containing redundant
license text that is already present in the LICENSE file.

**The Solution**: SPDX-License-Identifier tags provide machine-readable license identification
in a minimal format. Per-file headers should be 2-3 lines maximum.

### Standard Minimal Header

For all C/C++ source files (.c, .cpp, .h):

```c
/* SPDX-License-Identifier: PHP-3.01
 * SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS) */
```

For shell scripts:

```bash
# SPDX-License-Identifier: PHP-3.01
# SPDX-FileCopyrightText: The PHP Group and contributors (see CREDITS)
```

### Why This Works

1. **Legal Compliance**: PHP License 3.01 requires the LICENSE file to be preserved in
   distributions but does NOT require per-file header text
2. **Machine Readable**: SPDX identifiers are the industry standard for automated license detection
3. **Centralized Attribution**: CREDITS file contains comprehensive author list with git history links
4. **24 → 2 Lines**: 91.7% reduction in header size following Datensparsamkeit principle

### Centralized Attribution (CREDITS file)

All author attribution is maintained in the CREDITS file, which includes:
- Current maintainers
- Original authors (InterBase extension)
- Links to full contributor graphs on GitHub

Git history (`git blame`, `git log`) provides authoritative per-file contribution tracking.

### Migration Notes

When updating legacy headers to minimal SPDX format:
1. Remove the verbose license box (24 lines)
2. Remove per-file author lists (centralized in CREDITS)
3. Keep only the 2-line SPDX header
4. Preserve any file-specific design decision comments

---

## Resources

- [TechTarget: Code Comment Best Practices (2025)](https://www.techtarget.com/searchsoftwarequality/tip/Code-comment-best-practices-every-developer-should-know)
- [Keep a Changelog](https://keepachangelog.com/)
- [Semantic Versioning](https://semver.org/)
- [PHP Extension Writing Standard](https://wiki.php.net/internals/extensions)
- [SPDX License List](https://spdx.org/licenses/)
- [REUSE Software Best Practices](https://reuse.software/spec/)

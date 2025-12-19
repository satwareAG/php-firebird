# Issue: Transaction.php uses fbird_trans_begin() which may not be autoloaded

**Created**: 2025-12-19
**Priority**: Medium
**Status**: Open
**Type**: Bug - Potential autoload issue

## Problem Description

The `src/Firebird/Transaction.php` file was modified to call `fbird_trans_begin()` instead of `fbird_trans()`:

```php
// Line 86 in Transaction.php
$resource = fbird_trans_begin($conn, FBIRD_DEFAULT);
```

However, `fbird_trans_begin()` is **not** a native C extension function - it's a PHP wrapper function defined in `src/Firebird/functions.php`:

```php
namespace Firebird;

function fbird_trans_begin(mixed $link, int $flags = FBIRD_DEFAULT, ?int $lockTimeout = null): mixed
{
    // ... wrapper around fbird_trans()
}
```

## Root Cause

1. Both files are in `namespace Firebird;`
2. PHP allows calling `fbird_trans_begin()` without prefix since they share namespace
3. BUT: PHP doesn't autoload functions - only classes via PSR-4
4. `functions.php` must be explicitly included/required before use

## Why Tests Pass Locally

Local tests may pass because:
- Test matrix script may include functions.php somewhere
- Composer autoload might be configured differently
- Test bootstrap may include the functions file

## Fix Options

### Option 1: Use native function (RECOMMENDED - minimal change)

Revert to using the native C function directly:

```php
// Transaction.php line 86
$resource = fbird_trans(FBIRD_DEFAULT, $conn);  // Native C function
```

### Option 2: Explicitly include functions.php

Add at top of Transaction.php:
```php
require_once __DIR__ . '/functions.php';
```

### Option 3: Configure Composer autoload

Add to `composer.json`:
```json
{
    "autoload": {
        "files": [
            "src/Firebird/functions.php"
        ]
    }
}
```

## Affected Code

**File**: `src/Firebird/Transaction.php`
**Method**: `Transaction::begin()`
**Line**: ~86

### Current (potentially broken):
```php
$resource = fbird_trans_begin($conn, FBIRD_DEFAULT);
```

### Should be:
```php
$resource = fbird_trans(FBIRD_DEFAULT, $conn);
```

## Verification Steps

1. Check CI pipeline for actual failure message
2. Verify if functions.php is being loaded anywhere
3. Apply fix
4. Run full test matrix: `scripts/host/test_matrix.sh`
5. Push and verify CI passes

## Related Files

- `src/Firebird/Transaction.php` - Uses the function
- `src/Firebird/functions.php` - Defines the wrapper function
- `phpstan/fbird-functions.stub.php` - PHPStan stub for the function
- `composer.json` - Autoloading configuration

## Commit to Fix

```bash
git checkout feature/fbird-extension-release

# Edit Transaction.php line 86, change:
# FROM: $resource = fbird_trans_begin($conn, FBIRD_DEFAULT);
# TO:   $resource = fbird_trans(FBIRD_DEFAULT, $conn);

git add src/Firebird/Transaction.php
git commit --amend --no-edit
git push --force-with-lease
```

## Additional Context

The `fbird_trans_begin()` wrapper was created to provide a cleaner API:
- Non-variadic (array-based)
- Named parameters
- Lock timeout support

However, `Transaction::begin()` doesn't need these features - it just needs basic transaction start with default flags.

## References

- Commit: 2ae9c64 (feat(oo-wrapper): add Transaction::getId() and getInfo() methods)
- Original function: `fbird_trans()` in C extension
- Wrapper location: `src/Firebird/functions.php`

# Upstream "FIXED" Issues - Deep Inspection Report

> **Analysis Date**: 2025-12-17  
> **Methodology**: Baby Steps™ incremental validation  
> **Scope**: Issues #82, #85, #86, #98 from upstream FirebirdSQL/php-firebird

---

## Executive Summary

| Issue | Status | Completeness | Further Action |
|-------|--------|--------------|----------------|
| #82 FBIRD Migration | ✅ FIXED | 95% | Cosmetic: phpinfo display strings |
| #85 README Tidy | ✅ FIXED | 90% | Add: PDO_Firebird comparison |
| #86 CI/CD Suite | ✅ FIXED | 100% | None needed |
| #98 Date Parsing | ⚠️ PARTIAL | 60% | CRITICAL: strptime migration |

---

## Issue #82: Migrate from IBASE to FBIRD Params

**Upstream Request**: Rename all `ibase_*` functions, `IBASE_*` constants, and `ibase.*` INI directives.

### Verification Results

#### INI Directives (14 total) - ✅ COMPLETE

All 14 INI directives properly renamed in `firebird.c`:

| Old Name | New Name | Status |
|----------|----------|--------|
| `ibase.allow_persistent` | `fbird.allow_persistent` | ✅ |
| `ibase.max_persistent` | `fbird.max_persistent` | ✅ |
| `ibase.max_links` | `fbird.max_links` | ✅ |
| `ibase.default_db` | `fbird.default_db` | ✅ |
| `ibase.default_user` | `fbird.default_user` | ✅ |
| `ibase.default_password` | `fbird.default_password` | ✅ |
| `ibase.default_charset` | `fbird.default_charset` | ✅ |
| `ibase.timestampformat` | `fbird.timestampformat` | ✅ |
| `ibase.dateformat` | `fbird.dateformat` | ✅ |
| `ibase.timeformat` | `fbird.timeformat` | ✅ |
| `ibase.default_trans_params` | `fbird.default_trans_params` | ✅ |
| `ibase.default_lock_timeout` | `fbird.default_lock_timeout` | ✅ |
| `ibase.blob_segment_size` | `fbird.blob_segment_size` | ✅ |
| `ibase.enable_exceptions` | `fbird.enable_exceptions` | ✅ |

#### Constants - ✅ COMPLETE (Intentional BC Break)

All constants registered with `FBIRD_*` prefix only. **No `IBASE_*` aliases** - this is intentional per design.

```c
// firebird.c - FBIRD_* constants only
REGISTER_LONG_CONSTANT("FBIRD_DEFAULT", PHP_FBIRD_DEFAULT, CONST_PERSISTENT);
REGISTER_LONG_CONSTANT("FBIRD_CREATE", PHP_FBIRD_CREATE, CONST_PERSISTENT);
REGISTER_LONG_CONSTANT("FBIRD_TEXT", PHP_FBIRD_FETCH_BLOBS, CONST_PERSISTENT);
// ... (25+ constants, all FBIRD_*)
```

#### Functions - Not Applicable

Functions are `fbird_*` only. The satwareAG fork does **not** provide `ibase_*` aliases. Users must update their code.

### Minor Issue Found: phpinfo() Display Strings

**Location**: `firebird.c` function `php_fbird_trans_displayer()`

The phpinfo() output for transaction parameters still displays `IBASE_*` names:

```c
// firebird.c lines ~830-860
PUTS_TP("IBASE_READ");      // Should be FBIRD_READ
PUTS_TP("IBASE_WRITE");     // Should be FBIRD_WRITE
PUTS_TP("IBASE_COMMITTED"); // Should be FBIRD_COMMITTED
// etc.
```

**Impact**: Cosmetic only - does not affect functionality.  
**Priority**: Low  
**Recommendation**: Update display strings to `FBIRD_*` for consistency.

---

## Issue #85: Tidy Up Landing Page (README.md)

**Upstream Request**: 
1. Move build instructions elsewhere
2. Start with quick project info
3. Show PHP/FB/Arch matrix graphically
4. Compare with PDO_Firebird

### Verification Results

#### Content Structure - ✅ EXCELLENT

The README.md is comprehensive (650+ lines) with:

| Section | Status | Quality |
|---------|--------|---------|
| Quick Start | ✅ Present | Excellent - Docker, Native, macOS |
| Features | ✅ Present | Complete list |
| Requirements | ✅ Present | Clear system/platform matrix |
| Usage Examples | ✅ Present | 4 code examples |
| Migration Guide | ✅ Present | Comprehensive function mapping |
| Configuration | ✅ Present | INI settings documented |
| Testing | ✅ Present | Test commands included |
| Troubleshooting | ✅ Present | Common issues addressed |
| Function Reference | ✅ Present | All 50+ functions listed |
| Version Compatibility | ✅ Present | Clear PHP/FB matrix |

#### Missing: PDO_Firebird Comparison

The upstream issue specifically requested a comparison with PDO_Firebird. This section is **not present** in the current README.

**Recommendation**: Add section:

```markdown
## PDO_Firebird vs php-firebird Extension

| Feature | PDO_Firebird | php-firebird |
|---------|--------------|--------------|
| API Style | PDO (database-agnostic) | Native (Firebird-specific) |
| Function Prefix | `$pdo->method()` | `fbird_*()` |
| Events Support | ❌ No | ✅ Yes |
| Service API | ❌ No | ✅ Full (backup, restore, users) |
| Array Fields | ❌ No | ✅ Yes |
| BLOB Streaming | ✅ Via LOB | ✅ Native + Streams |
| Prepared Statements | ✅ Yes | ✅ Yes |
| Transaction Control | ✅ Basic | ✅ Advanced (savepoints, TPB) |
| Named Cursors | ❌ No | ✅ Yes (fbird_name_result) |
| Generator/Sequence | Via SQL only | ✅ Native fbird_gen_id() |
| Modern OO API | ❌ Legacy C API | ✅ FB 3.0+ OO API |

**When to use PDO_Firebird:**
- Building database-agnostic applications
- Simple CRUD operations
- Portability across databases is priority

**When to use php-firebird:**
- Firebird-specific features needed (events, service API)
- Array field support required
- Advanced transaction control (savepoints, table locking)
- Performance-critical applications
- Need modern Firebird 3.0+ OO API benefits
```

---

## Issue #86: Automatic Build and Testing Suite

**Upstream Request**: GitHub Actions CI/CD pipeline with PHP/Firebird version matrix.

### Verification Results - ✅ COMPLETE

#### Workflow Files (5 total)

| Workflow | Purpose | Status |
|----------|---------|--------|
| `main.yml` | PHP × Firebird matrix testing | ✅ 20 combinations |
| `code-quality.yml` | Static analysis (clang-tidy, cppcheck) | ✅ |
| `codeql.yml` | Security scanning | ✅ |
| `coverage.yml` | Code coverage reporting | ✅ |
| `sanitizers.yml` | AddressSanitizer memory safety | ✅ |

#### Test Matrix Coverage

```yaml
strategy:
  matrix:
    php-version: ['8.1', '8.2', '8.3', '8.4', '8.5']
    firebird-version: ['2.5', '3.0', '4.0', '5.0']
```

**Total: 20 combinations** (5 PHP versions × 4 FB versions)

#### Quality Tools Integration

| Tool | Purpose | Integration |
|------|---------|-------------|
| clang-tidy | C++ linting | ✅ `.clang-tidy` |
| cppcheck | Static analysis | ✅ `.cppcheck` |
| Gitleaks | Secret scanning | ✅ `.gitleaks.toml` |
| CodeQL | Security SAST | ✅ `codeql.yml` |
| AddressSanitizer | Memory safety | ✅ `sanitizers.yml` |

**Assessment**: CI/CD is **production-grade** and exceeds upstream requirements.

---

## Issue #98: Modern Date/Time/Timestamp Format Parsing

**Upstream Request**: Address strptime() deprecation and platform-specific issues.

### Verification Results - ⚠️ PARTIAL FIX

#### INI Directive Renamed - ✅ DONE

INI directive renamed from `ibase.timestampformat` to `fbird.timestampformat`.

#### strptime() Still Used - ❌ NOT FIXED

**Critical Finding**: `strptime()` is still used extensively in the codebase.

| File | Usage Count | Lines |
|------|-------------|-------|
| `fbird_query_array.c` | 5 | Date/time parsing for array binding |
| `fbird_query_bind.c` | 4 | Parameter binding parsing |
| `fbird_query_exec.c` | 1 | (define only) |
| `fbird_result.c` | 1 | (define only) |
| `fbird_metadata.c` | 1 | (define only) |

#### strptime() Issues

**Platform-Specific Behavior:**
- Windows: Not available in standard library (requires workaround)
- macOS: Different behavior than Linux
- musl libc: Stricter parsing than glibc

**Example problematic code** (`fbird_query_bind.c`):
```c
if (!strptime(Z_STRVAL_P(b_var), format, &t)) {
    /* strptime() cannot handle it, so let IB have a try */
    break;
}
```

### Recommended Fix

Replace `strptime()` with cross-platform PHP DateTime parsing:

**Option 1: PHP DateTime API (via Zend)**
```c
// Use php_date_initialize() and php_date_parse_*() functions
#include "ext/date/php_date.h"

// Parse date string using PHP's DateTime
php_date_obj *date_obj;
zval datetime_zval;
object_init_ex(&datetime_zval, php_date_get_date_ce());
date_obj = Z_PHPDATE_P(&datetime_zval);
if (!php_date_initialize(date_obj, date_string, date_len, format, NULL, 0)) {
    // Parsing failed
}
```

**Option 2: Custom cross-platform parser**
```c
// Implement simple ISO-8601 parser
int parse_iso_datetime(const char *str, int *year, int *month, int *day,
                       int *hour, int *minute, int *second) {
    return sscanf(str, "%4d-%2d-%2d %2d:%2d:%2d",
                  year, month, day, hour, minute, second) == 6;
}
```

**Option 3: Keep strptime() for POSIX + fallback**
```c
#ifdef HAVE_STRPTIME
    // Use strptime on POSIX systems
    strptime(str, format, &t);
#else
    // Windows/portable fallback using sscanf or php_date_*
    if (!parse_iso_datetime(str, ...)) {
        // Let Firebird handle it
    }
#endif
```

**Priority**: MEDIUM - current code works but has portability warnings  
**Effort**: 2-4 hours to implement cross-platform solution  
**Impact**: Better Windows compatibility, eliminates deprecation warnings

---

## Improvement Recommendations Summary

### High Priority

1. **Issue #98**: Migrate strptime() to cross-platform solution
   - Impact: Windows compatibility, POSIX compliance
   - Effort: 4 hours
   - Files: `fbird_query_bind.c`, `fbird_query_array.c`

### Low Priority

2. **Issue #82**: Update phpinfo() display strings
   - Impact: Cosmetic consistency
   - Effort: 15 minutes
   - File: `firebird.c` function `php_fbird_trans_displayer()`

3. **Issue #85**: Add PDO_Firebird comparison section
   - Impact: User education
   - Effort: 30 minutes
   - File: `README.md`

---

## Verification Commands

```bash
# Verify INI directives
php -d extension=./modules/firebird.so -r 'print_r(ini_get_all("fbird"));'

# Verify constants
php -d extension=./modules/firebird.so -r 'print_r(get_defined_constants(true)["fbird"] ?? []);'

# Check strptime usage
grep -rn "strptime" --include="*.c" .

# Run full test suite
make test TESTS=tests/
```

---

## Files Modified Reference

For implementing recommendations:

| Recommendation | Files to Modify |
|---------------|-----------------|
| strptime migration | `fbird_query_bind.c`, `fbird_query_array.c` |
| phpinfo display | `firebird.c` |
| README comparison | `README.md` |

---

*Document generated as part of upstream issue analysis. See `docs/UPSTREAM_ISSUE_ANALYSIS.md` for full 19-issue overview.*

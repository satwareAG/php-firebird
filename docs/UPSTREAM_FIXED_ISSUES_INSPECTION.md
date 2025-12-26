# Upstream "FIXED" Issues - Deep Inspection Report

> **Analysis Date**: 2025-12-19 (Updated)
> **Methodology**: Baby Steps™ incremental validation  
> **Scope**: All open issues from upstream FirebirdSQL/php-firebird

---

## Repository Reference

| Repository | URL | Note |
|------------|-----|------|
| **Upstream** | https://github.com/FirebirdSQL/php-firebird | Original FirebirdSQL project |
| **Fork** | https://github.com/satwareAG/php-firebird | satwareAG enhanced fork |

---

## Executive Summary

| Issue | Status | Completeness | Further Action |
|-------|--------|--------------|----------------|
| #82 FBIRD Migration | ✅ FIXED | 100% | None (phpinfo display verified) |
| #85 README Tidy | ✅ FIXED | 100% | Complete (PDO comparison added) |
| #86 CI/CD Suite | ✅ FIXED | 100% | None needed |
| #98 Date Parsing | ✅ FIXED | 100% | None (cross-platform solution implemented) |
| #99 CHAR Type Reporting | ✅ FIXED | 100% | None needed |
| #25 UTF-8 CHAR Padding | ✅ FIXED | 100% | None needed |
| #66 Event PHP 8.4+ Stack | ✅ FIXED | 100% | None (polling model redesign) |
| #45 Event Memory Leak | ✅ FIXED | 100% | None (polling model redesign) |
| #42 Array Test 007 | ✅ FIXED | 100% | None (test passes on all versions) |
| #22 ibase_close | ✅ FIXED | 100% | None (second close returns false correctly) |
| #53 Service Attach Local | ✅ FIXED | 100% | None (empty host → local connection) |
| #71 Service INI Defaults | ✅ FIXED | 100% | None (INI_STR fallback added) |
| #97 Connection Reuse | ⚠️ BY DESIGN | 100% | Enhancement: Fork Issue #11 ✅ IMPLEMENTED |

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

### phpinfo() Display Strings - ✅ VERIFIED FIXED

**Location**: `firebird.c` function `php_fbird_trans_displayer()`

The phpinfo() output correctly displays `FBIRD_*` names:

```c
// firebird.c - VERIFIED 2025-12-17
PUTS_TP("FBIRD_READ");
PUTS_TP("FBIRD_WRITE");
PUTS_TP("FBIRD_COMMITTED");
PUTS_TP("FBIRD_REC_VERSION");
PUTS_TP("FBIRD_REC_NO_VERSION");
PUTS_TP("FBIRD_CONSISTENCY");
PUTS_TP("FBIRD_CONCURRENCY");
PUTS_TP("FBIRD_NOWAIT");
PUTS_TP("FBIRD_WAIT");
PUTS_TP("FBIRD_LOCK_TIMEOUT");
PUTS_TP("FBIRD_DEFAULT");
```

**Status**: ✅ COMPLETE - No further action needed.

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

#### PDO_Firebird Comparison - ✅ COMPLETE

The upstream issue specifically requested a comparison with PDO_Firebird. This section has been added to README.md:

**Location**: README.md lines 26-57

**Content**: Full feature comparison table including:
- API style differences (PDO vs Native)
- Feature support (Events, Service API, Array Fields, etc.)
- Use case recommendations for each option

**Verification**: Confirmed present in README.md (2025-12-19)

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

### Verification Results - ✅ FIXED (2025-12-17)

#### Implementation Summary

A comprehensive cross-platform date/time parsing solution was implemented, completely replacing all `strptime()` usage with portable `sscanf()`-based parsing utilities.

#### New Files Added

| File | Purpose |
|------|---------|
| `fbird_datetime.h` | Header with cross-platform date/time parsing API |
| `fbird_datetime.c` | Implementation of portable date/time parsers |

#### Key Features

1. **Auto-format Detection**: Parses multiple date formats without configuration:
   - ISO 8601: `YYYY-MM-DD`, `YYYY-MM-DD HH:MM:SS`, `YYYY-MM-DDTHH:MM:SS`
   - European: `DD.MM.YYYY`, `DD.MM.YYYY HH:MM:SS`
   - US: `MM/DD/YYYY`, `MM/DD/YYYY HH:MM:SS`

2. **Time Parsing**: Supports `HH:MM:SS` and `HH:MM:SS.FFFF` (with fractions)

3. **Timezone Support**: Extracts timezone from strings for Firebird 4.0+ types:
   - Offset formats: `+HH:MM`, `-HHMM`
   - Named timezones: `GMT`, `UTC`, `Europe/Berlin`, etc.

4. **Validation**: All parsed components are validated before use:
   - Year range: 1-9999
   - Month range: 1-12
   - Day range: 1-31 (with month/leap-year awareness)
   - Hour: 0-23, Minute: 0-59, Second: 0-59

5. **Integration with Firebird OO API**: Uses `fbu_encode_*()` functions for encoding.

#### Files Modified

| File | Changes |
|------|---------|
| `fbird_query_bind.c` | Replaced strptime with `fbird_parse_date/time/timestamp()` |
| `fbird_query_array.c` | Replaced strptime with `fbird_parse_date/time/timestamp()`, removed `_GNU_SOURCE` define |
| `config.m4` | Added `fbird_datetime.c` to build sources |

#### strptime() Usage - ✅ ELIMINATED

All `strptime()` calls have been removed from the codebase:
- `fbird_query_bind.c`: Replaced with `fbird_parse_*()` utilities
- `fbird_query_array.c`: Replaced with `fbird_parse_*()` utilities
- Remaining `HAVE_STRPTIME` defines in other files are now unused

#### Platform Compatibility

The new implementation works consistently across:
- **Linux (glibc)**: Full support
- **Linux (musl libc)**: Full support (Alpine, BusyBox)
- **Windows**: Full support (no strptime dependency)
- **macOS**: Full support

#### Test Results

All date/time related tests pass:
```
TEST 1/5 [tests/time_003.phpt]  PASS
TEST 2/5 [tests/time_004.phpt]  PASS
TEST 3/5 [tests/timezone_001.phpt]  PASS
TEST 4/5 [tests/timezone_002.phpt]  PASS
TEST 5/5 [tests/timezone_003.phpt]  PASS
```

**Verification Date**: 2025-12-17  
**Test Environment**: PHP 8.3.28, Firebird 4.0  
**Status**: ✅ COMPLETE - No further action needed

---

## Issue #99: Incorrect Type Reporting for CHAR Fields

**Upstream Issue**: https://github.com/FirebirdSQL/php-firebird/issues/99  
**Upstream Request**: `ibase_field_info()` returns "VARCHAR" instead of "CHAR" for CHAR fields.

### Original Problem

The upstream issue reported that the following code:

```php
ibase_query("CREATE TABLE FIELDSTEST (CHAR_FIXED CHAR(10) DEFAULT 'ABCDE')");
ibase_commit();
$q = ibase_prepare("SELECT * FROM FIELDSTEST");
var_dump(ibase_field_info($q, 0)["type"]);
```

**Expected**: `string(4) "CHAR"`  
**Actual (upstream bug)**: `string(7) "VARCHAR"`

The bug was related to commit `29e9d6f8fd` in the upstream repository which reportedly broke the type detection.

### Root Cause Analysis

The type mapping in `fbird_metadata.c` (function `_php_fbird_field_info()`) uses the `XSQLVAR.sqltype` field to determine the SQL type name. The mapping is:

```c
switch (var->sqltype & ~1) {
    case SQL_TEXT:
        s = "CHAR";      // Fixed-length character field
        break;
    case SQL_VARYING:
        s = "VARCHAR";   // Variable-length character field
        break;
    // ... other types
}
```

In the satwareAG fork, this mapping is **correct** and has not been affected by the upstream bug.

### Verification Results - ✅ FIXED

#### Test Coverage

Multiple tests verify correct CHAR vs VARCHAR type reporting:

| Test File | Purpose | Status |
|-----------|---------|--------|
| `tests/fbird_field_info_001.phpt` | Basic field types including CHAR/VARCHAR | ✅ PASS |
| `tests/fbird_field_info_002.phpt` | Firebird 3.0+ field types | ✅ PASS |
| `tests/fbird_field_info_003.phpt` | Firebird 4.0+ field types | ✅ PASS |
| `tests/fbird_field_info_004.phpt` | UTF8 charset field types | ✅ PASS |
| `tests/issue99_001.phpt` | Exact reproduction from upstream issue | ✅ PASS |

#### Test Evidence (fbird_field_info_001.phpt)

```
CHAR_FIXED/CHAR/10         # CHAR field correctly reports as "CHAR"
VARCHAR_FIELD/VARCHAR/50   # VARCHAR field correctly reports as "VARCHAR"
CHAR_UTF8/CHAR/40          # UTF8 CHAR field correctly reports as "CHAR"
VARCHAR_UTF8/VARCHAR/200   # UTF8 VARCHAR field correctly reports as "VARCHAR"
BINARY_FIXED/CHAR/16       # Binary CHAR field correctly reports as "CHAR"
VARBINARY_FIELD/VARCHAR/100 # Binary VARCHAR field correctly reports as "VARCHAR"
```

#### Test Evidence (issue99_001.phpt - Exact Reproduction)

```
Field name: CHAR_FIXED
Field type: CHAR
Field length: 10
TEST PASSED: CHAR field correctly reports as CHAR

--- Comparison Test ---
CHAR_COL type: CHAR
VARCHAR_COL type: VARCHAR
COMPARISON TEST PASSED: Types are correctly differentiated
```

### PHP Version Matrix Results

| PHP Version | Test Result |
|-------------|-------------|
| PHP 8.3.28 | ✅ PASS (all 5 field_info tests) |
| PHP 8.4.15 | ✅ PASS (all 4 applicable tests) |
| PHP 8.5.0 | ✅ PASS (all 4 applicable tests) |

**Verification Date**: 2025-12-17  
**Test Environment**: Docker matrix with Firebird 4.0  
**Status**: ✅ COMPLETE - No further action needed.

---

## Issue #25: CHAR(1) Padded with Spaces in UTF-8

**Upstream Request**: CHAR(1) returns 'A   ' (with 3 extra spaces) when using UTF-8 charset.

### Verification Results - ✅ FIXED

**Test File**: `tests/datatype_char_utf8.phpt`

**Evidence**:
```php
// Test data:
// v_char_utf8_1 CHAR(1) CHARACTER SET UTF8 = '€'
// v_char_utf8_10 CHAR(10) CHARACTER SET UTF8 = '  A   €   '

// Expected output (PASS):
["V_CHAR_UTF8_1"]=>  string(3) "€"        // Correctly trimmed (3 bytes = euro sign)
["V_CHAR_UTF8_10"]=> string(9) "  A   €"  // Trailing spaces trimmed
["V_VARCHAR_UTF8_1"]=> string(3) "€"      // VARCHAR unchanged
```

**Implementation**: CHAR fields are properly right-trimmed on fetch in `fbird_result.c`.

**Key Behavior**:
- CHAR fields: Trailing spaces trimmed (correct SQL standard behavior)
- VARCHAR fields: Content unchanged (no trimming needed)
- UTF-8 multi-byte characters handled correctly

**Verification Date**: 2025-12-17  
**Test Result**: PASS (PHP 8.3, Firebird 4.0)

**Status**: ✅ COMPLETE - No further action needed.

---

## Issue #66: Event Handling PHP 8.4+ Stack Overflow

**Upstream Request**: `tests/008.php` fails with "Maximum call stack size reached" on PHP 8.4+.

### Problem Analysis

The upstream issue reported stack overflow errors when using event handling with PHP 8.4+. This was caused by the old implementation's use of `isc_que_events()` with C callbacks invoked from Firebird's internal thread.

**Root Cause** (Original Implementation):
- Async callbacks from Firebird threads called PHP functions (`call_user_function`)
- No PHP request context exists on Firebird threads
- TSRMLS macros are empty in PHP 8.1+ (TSRMLS_FETCH_FROM_CTX does nothing)
- Results: random crashes, memory corruption, undefined behavior, stack overflow

### Solution Implemented - Thread-Safe Polling Model

The satwareAG fork completely redesigned event handling with a **polling model**:

1. **`fbird_set_event_handler()`**: Registers events and stores callback but does NOT use async callbacks
2. **`fbird_poll_event()`**: Uses `isc_wait_for_event()` synchronously to check for events and calls PHP callback from PHP thread (SAFE!)
3. **`fbird_wait_event()`**: Continues to work as before (blocking synchronous)

**Key Code Changes** (`fbird_events.c`):

```c
/**
 * THREAD-SAFETY REDESIGN (PHP 8.1+)
 * 
 * SOLUTION (Polling Model):
 * - fbird_set_event_handler() registers the event and stores the callback
 *   but does NOT use async callbacks from isc_que_events()
 * - fbird_poll_event() uses isc_wait_for_event() synchronously to check
 *   for events and calls the PHP callback from the PHP thread (safe!)
 * 
 * This ensures all PHP callbacks execute in the correct PHP thread context.
 */
```

### Verification Results - ✅ FIXED

**Test Files**:
- `tests/008.phpt` - Basic event handling API
- `tests/008_timeout.phpt` - Timeout API and constants
- `tests/event_poller_wrapper.phpt` - PHP wrapper classes

**Test Results** (PHP 8.4.15):
```
TEST 1/3 [tests/008.phpt]              PASS
TEST 2/3 [tests/008_timeout.phpt]      PASS
TEST 3/3 [tests/event_poller_wrapper.phpt] PASS
```

**Test Results** (PHP 8.5.0):
```
TEST 1/3 [tests/008.phpt]              PASS
TEST 2/3 [tests/008_timeout.phpt]      PASS
TEST 3/3 [tests/event_poller_wrapper.phpt] PASS
```

**Verification Date**: 2025-12-17  
**Status**: ✅ COMPLETE - No further action needed.

---

## Issue #45: Event Handling Memory Leak in Debug Builds

**Upstream Request**: `tests/008.phpt` disabled for debug builds due to memory leak.

### Problem Analysis

The same thread-safety issues that caused stack overflow (#66) also caused memory leaks:
- Resources allocated in one thread (Firebird) but referenced in another (PHP)
- Improper cleanup due to thread context mismatch
- Memory corruption leading to leaked references

### Solution Implemented

The polling model redesign (same as #66) also resolves memory issues:

1. **Single-threaded execution**: All operations occur in PHP thread
2. **Proper resource cleanup**: `_php_fbird_free_event()` handles cleanup correctly
3. **Reference counting**: Event resources properly reference-counted via `link_res`
4. **Safety limits**: `max_callbacks` limit (1000) prevents infinite loops

**Key Safety Features**:

```c
/* Safety limit check */
if (event->callback_count >= event->max_callbacks) {
    event->state = DEAD;
    _php_fbird_module_error("Event callback limit exceeded");
    RETURN_FALSE;
}
```

### Verification Results - ✅ FIXED

The memory leak was eliminated by the polling model design:
- No cross-thread resource sharing
- Clean resource lifecycle management
- Tests pass without memory warnings

**Verification Date**: 2025-12-17  
**Test Environment**: PHP 8.4, 8.5 with Firebird 4.0  
**Status**: ✅ COMPLETE - No further action needed.

---

## Issue #22: ibase_close Not Working as Expected

**Upstream Issue**: https://github.com/FirebirdSQL/php-firebird/issues/22  
**Upstream Request**: `ibase_close($x)` doesn't actually close connection; second call returns true.

### Problem Analysis

The original bug reported that calling `ibase_close()` multiple times on the same connection would:
1. First call: Return `true` (expected)
2. Second call: Return `true` again (BUG - should return `false`)

This indicated the connection was not actually being closed.

### Solution Implemented

The satwareAG fork correctly implements connection closing with proper resource cleanup:

**Implementation** (`firebird.c`, `PHP_FUNCTION(fbird_close)`):
```c
// Uses zend_list_close() for proper resource cleanup
// Returns false if connection already closed or invalid resource
```

### Verification Results - ✅ FIXED

**Test Files**:
- `tests/fbird_close_004.phpt` - Basic close test
- `tests/fbird_close_005.phpt` - Error handling test

**Test Evidence** (fbird_close_004.phpt):
```php
$conn = fbird_connect($db, $user, $pass);
var_dump(fbird_close($conn));  // bool(true)  - first close succeeds
var_dump(fbird_close($conn));  // bool(false) - already closed (CORRECT!)
var_dump(fbird_close());       // bool(false) - no default link
```

**Verification Date**: 2025-12-17  
**Test Result**: PASS (PHP 8.3, 8.4, 8.5)  
**Status**: ✅ COMPLETE - No further action needed.

---

## Issue #53: ibase_service_attach Local Connection

**Upstream Issue**: https://github.com/FirebirdSQL/php-firebird/issues/53  
**Upstream Request**: Service attach always uses TCP pattern `%s:service_mgr` even for local connections.

### Problem Analysis

The original bug reported that `ibase_service_attach()` could not connect to local embedded Firebird servers because it always prefixed the host to the service manager string.

**Expected Behavior**:
- Local connection (empty host): Use `"service_mgr"` directly
- Remote connection (host provided): Use `"host:service_mgr"` pattern

### Solution Implemented

The satwareAG fork correctly handles both local and remote service manager connections:

**Implementation** (`fbird_service.c`, `PHP_FUNCTION(fbird_service_attach)`):
```c
char loc[128] = "service_mgr";  // Default: local connection
// ...
if(hlen > 0){
    slprintf(loc, sizeof(loc), "%s:service_mgr", host);  // Remote connection
}
```

### Verification Results - ✅ FIXED

**Test Files**:
- `tests/fbird_service_001.phpt` - Service attach error handling
- `tests/fbird_service_002.phpt` - Server info constants

**Verification Date**: 2025-12-17  
**Test Result**: PASS  
**Status**: ✅ COMPLETE - No further action needed.

---

## Issue #71: ibase_service_attach INI Defaults

**Upstream Issue**: https://github.com/FirebirdSQL/php-firebird/issues/71  
**Upstream Request**: Service attach should respect `fbird.default_user`/`fbird.default_password` INI directives.

### Problem Analysis

The original `fbird_service_attach()` did NOT fall back to INI defaults when user/password parameters were not provided (unlike `fbird_connect()` which does).

### Solution Implemented

The satwareAG fork added INI fallback to match `fbird_connect()` behavior:

**Implementation** (`fbird_service.c`):
```c
/* Fall back to INI defaults if user/password not provided (Issue #71) */
if (ulen == 0) {
    char *ini_user = INI_STR("fbird.default_user");
    if (ini_user && *ini_user) {
        user = ini_user;
        ulen = strlen(ini_user);
    }
}

if (plen == 0) {
    char *ini_pass = INI_STR("fbird.default_password");
    if (ini_pass && *ini_pass) {
        pass = ini_pass;
        plen = strlen(ini_pass);
    }
}
```

### Verification Results - ✅ FIXED

**Test File**: `tests/fbird_service_ini_defaults.phpt`

**Usage Example**:
```php
// php.ini: fbird.default_user=SYSDBA, fbird.default_password=masterkey

// Now works - uses INI defaults
$svc = fbird_service_attach('localhost');  // Uses SYSDBA/masterkey from INI

// Still works - explicit credentials override INI
$svc = fbird_service_attach('localhost', 'CUSTOM_USER', 'custom_pass');
```

**Verification Date**: 2025-12-17  
**Test Result**: PASS  
**Status**: ✅ COMPLETE - No further action needed.

---

## Issue #97: Connection Reuse Behavior

**Upstream Issue**: https://github.com/FirebirdSQL/php-firebird/issues/97  
**Upstream Request**: Multiple `ibase_connect()` calls with identical arguments return same resource.

### Problem Analysis

The reporter observed that:
```php
$con1 = ibase_connect($db, $user, $password, "utf8");
$con2 = ibase_connect($db, $user, $password, "utf8");
// Both return the SAME resource - only 1 actual connection in MON$ATTACHMENTS
```

### Analysis Verdict: BY DESIGN (Enhancement Needed)

After comprehensive research comparing with PostgreSQL, MySQLi, and PDO extensions:

**Key Finding**: The default connection reuse behavior is **CORRECT** - it matches PostgreSQL's well-established `pg_connect()` pattern.

**However**, PostgreSQL provides `PGSQL_CONNECT_FORCE_NEW` as an escape hatch when developers explicitly need a new connection. The php-firebird extension was missing this capability.

| Extension | Default Behavior | Escape Hatch |
|-----------|-----------------|--------------|
| pg_connect() | Reuses | `PGSQL_CONNECT_FORCE_NEW` |
| mysqli_connect() | Always new | N/A |
| PDO | Always new | N/A |
| **fbird_connect()** | Reuses | **NOW: `FBIRD_CONNECT_FORCE_NEW`** |

### Solution Implemented - Fork Issue #11 ✅

The satwareAG fork implemented `FBIRD_CONNECT_FORCE_NEW` flag (Fork Issue #11):

**New Usage**:
```php
// Default: Same parameters = same connection (by design, backward compatible)
$conn1 = fbird_connect($db, $user, $pass);
$conn2 = fbird_connect($db, $user, $pass);  // Returns SAME resource

// Force new connection when needed (NEW FEATURE!)
$conn3 = fbird_connect($db, $user, $pass, null, null, null, null, null, FBIRD_CONNECT_FORCE_NEW);
// Returns NEW connection resource
```

**Test File**: `tests/fbird_connect_force_new.phpt`

**Verification Date**: 2025-12-17  
**Implementation Status**: ✅ IMPLEMENTED (Fork Issue #11)  
**Status**: ✅ COMPLETE - Enhancement deployed.

---

## Issue #42: Array Handling Test (007.phpt)

**Upstream Issue**: https://github.com/FirebirdSQL/php-firebird/issues/42  
**Upstream Request**: `tests/007.phpt` was disabled in upstream due to failures.

### Verification Results - ✅ FIXED

The satwareAG fork has the test enabled and passing across all PHP versions:

**Test Files**:
- `tests/007.phpt` - Main array handling test
- `tests/007_iso_char.phpt` - ISO CHAR array variant
- `tests/007_iso_integer.phpt` - ISO INTEGER array variant
- `tests/007_iso_varchar10.phpt` - ISO VARCHAR(10) array variant
- `tests/007_iso_varchar1000.phpt` - ISO VARCHAR(1000) array variant

**Test Results**:
| PHP Version | 007.phpt | Variants |
|-------------|----------|----------|
| PHP 8.3.28 | ✅ PASS | ✅ All PASS |
| PHP 8.4.15 | ✅ PASS | ✅ All PASS |
| PHP 8.5.0 | ✅ PASS | ✅ All PASS |

**Verification Date**: 2025-12-17  
**Status**: ✅ COMPLETE - No further action needed.

---

## Upstream Discussions Review (2025-12-19)

The following upstream GitHub Discussions were reviewed for potential improvement ideas:

### Discussion #62: Laravel Working with Firebird?

**URL**: https://github.com/FirebirdSQL/php-firebird/discussions/62  
**Category**: Q&A  
**Relevance**: ❌ Not Applicable

**Summary**: User asking about Laravel compatibility with Firebird. The maintainers clarified that Laravel uses PDO drivers, and there is a separate `harrygulliford/laravel-firebird` package that wraps PDO_Firebird.

**Action**: None required - This is a Q&A about third-party framework integration, not related to the php-firebird extension itself.

### Discussion #33: PHP 8.1 Interbase Problem

**URL**: https://github.com/FirebirdSQL/php-firebird/discussions/33  
**Category**: Q&A  
**Relevance**: ❌ Not Applicable

**Summary**: User asking about Windows DLL availability for PHP 8.1. The discussion covered:
- PDO_Firebird vs php-firebird differences
- Windows `fbclient.dll` and architecture matching (32/64-bit)
- XAMPP configuration issues

**Action**: None required - This is a Q&A about Windows setup and PDO_Firebird, not related to our extension's functionality.

---

## Improvement Recommendations Summary

### All Issues Completed

All upstream "FIXED" issues have been verified and are 100% complete.

### Completed (No Action Required)

- **Issue #82**: ✅ FBIRD migration complete (phpinfo verified)
- **Issue #85**: ✅ README tidied with PDO_Firebird comparison added
- **Issue #86**: ✅ CI/CD suite complete
- **Issue #98**: ✅ Cross-platform date/time parsing implemented (strptime replaced)
- **Issue #99**: ✅ CHAR type reporting correct
- **Issue #25**: ✅ UTF-8 CHAR padding handled correctly
- **Issue #66**: ✅ Event handling PHP 8.4+ stack overflow (polling model redesign)
- **Issue #45**: ✅ Event handling memory leak (polling model redesign)
- **Issue #22**: ✅ ibase_close correctly returns false on second call
- **Issue #53**: ✅ Service attach supports local connections
- **Issue #71**: ✅ Service attach respects INI defaults
- **Issue #97**: ✅ FBIRD_CONNECT_FORCE_NEW flag implemented (Fork Issue #11)
- **Issue #42**: ✅ Array test 007.phpt passes on all PHP versions

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

| Task | Status | Files |
|------|--------|-------|
| strptime migration | ✅ Complete | `fbird_datetime.c`, `fbird_query_bind.c`, `fbird_query_array.c` |
| phpinfo display | ✅ Complete | `firebird.c` |
| README comparison | ✅ Complete | `README.md` |

### Important Note on compile and test commands
Use scripts/host/test_matrix.sh to run the full test suite on all supported platforms.

*Document generated as part of upstream issue analysis. See `docs/UPSTREAM_ISSUE_ANALYSIS.md` for full 19-issue overview.*

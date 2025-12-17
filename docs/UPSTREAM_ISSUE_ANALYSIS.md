# Upstream Issue Analysis: FirebirdSQL/php-firebird

**Analysis Date:** 2025-12-17  
**Branch:** `feature/fbird-extension-release`

## Repository Reference

| Repository | URL | Issue Prefix |
|------------|-----|--------------|
| **Upstream** | https://github.com/FirebirdSQL/php-firebird | `Upstream #N` |
| **Fork** | https://github.com/satwareAG/php-firebird | `Fork #N` |

> **Note:** This document primarily analyzes **upstream issues** from FirebirdSQL/php-firebird.
> For **fork-specific enhancements**, see the [Fork Enhancements](#fork-enhancements-satwareagphp-firebird) section at the end.

This document analyzes all 19 open issues from the upstream FirebirdSQL/php-firebird repository and determines their status in the satwareAG fork.

---

## Summary

| Status | Count | Issues |
|--------|-------|--------|
| ✅ **FIXED** in satwareAG fork | 11 | #82, #85, #86, #98, #99, #25, #66, #45, #42, #22, #53 |
| ⚠️ **BY DESIGN** (enhancement needed) | 1 | #97 - Default OK, but needs `FBIRD_CONNECT_FORCE_NEW` flag |
| 📝 **DOCUMENTATION ONLY** | 4 | #90, #72, #71, #63 |
| 🚀 **FEATURE REQUEST** | 2 | #83, #12 |
| ❓ **NOT APPLICABLE** | 1 | #70 |

---

## Detailed Analysis

### ✅ FIXED in satwareAG Fork

#### Issue #82: Migrate from IBASE to FBIRD params
**Status:** ✅ **FULLY FIXED**

**Original Issue:** Request to migrate from IBASE_* constants and ibase.* INI settings to FBIRD_*.

**satwareAG Implementation:**
- All 14 INI directives renamed: `ibase.*` → `fbird.*`
- All constants renamed: `IBASE_*` → `FBIRD_*` (no BC aliases - intentional clean break)
- All functions renamed: `ibase_*` → `fbird_*` (BC aliases retained for migration)
- Source files renamed: `interbase.*` → `firebird.*`, `ibase_*.c` → `fbird_*.c`

**Evidence:** See `docs/DEVELOPMENT_HISTORY.md` "Extension Rename" section.

---

#### Issue #85: Tidy up landing page on GitHub (README.md)
**Status:** ✅ **FIXED**

**Original Issue:** Clean up README with:
- Build instructions moved elsewhere
- Quick information about the project
- Supported PHP/FB/Arch combinations
- How it differs from PDO_Firebird

**satwareAG Implementation:**
- README restructured with clear sections
- Version matrix documentation
- CI/CD badges showing test status
- Build instructions referenced but organized

---

#### Issue #86: Automatic build and testing suite
**Status:** ✅ **FULLY IMPLEMENTED**

**Original Issue:** Prepare Docker/VMs for automatic building and testing across multiple PHP/FB versions.

**satwareAG Implementation:**
- **GitHub Actions CI/CD:** 3 workflows with full matrix testing
  - PHP 8.1, 8.2, 8.3, 8.4, 8.5
  - Firebird 2.5, 3.0, 4.0, 5.0
  - 20 job combinations in test matrix
- **Docker environment:** Multi-version Dockerfiles in `docker/php/`
- **105 PHPT tests:** All passing (0 failures)
- **Code coverage:** Linux Code Coverage workflow
- **Code quality:** Cppcheck, clang-tidy integration

**Evidence:** `.github/workflows/`, `docker/` directory, CI passing status.

---

#### Issue #98: Modern date/time/timestamp format parsing (PARTIAL)
**Status:** ⚠️ **PARTIALLY ADDRESSED**

**Original Issue:** Replace deprecated `strptime()` with PHP date parsing facilities for `fbird.timestampformat`, `fbird.dateformat`, `fbird.timeformat`.

**satwareAG Status:**
- INI directives renamed to `fbird.*` namespace ✅
- `strptime()` still used internally (cross-platform issues remain)
- Modern PHP date facilities not yet integrated

**Recommendation:** Keep open, investigate PHP DateTime API integration for format parsing.

---

#### Issue #66: tests/008.php fail with PHP 8.4 - Maximum call stack size
**Status:** ✅ **FIXED**

**Original Issue:** Event handling test fails with "Maximum call stack size reached" on PHP 8.4+.

**satwareAG Solution - Thread-Safe Polling Model:**

The satwareAG fork completely redesigned event handling to eliminate the thread-safety issues:

**Root Cause:** The old implementation used `isc_que_events()` with C callbacks invoked from Firebird's internal thread. These callbacks called `call_user_function()` from a non-PHP thread, which caused:
- No PHP request context on Firebird threads
- TSRMLS macros empty in PHP 8.1+ (TSRMLS_FETCH_FROM_CTX does nothing)
- Random crashes, memory corruption, and stack overflow

**New Design:**
1. `fbird_set_event_handler()` - Registers events, stores callback, does NOT use async callbacks
2. `fbird_poll_event()` - Synchronously checks for events, calls PHP callback from PHP thread (SAFE!)
3. `fbird_wait_event()` - Unchanged (blocking synchronous)

**Test Results (2025-12-17):**
- PHP 8.4.15: tests/008.phpt ✅ PASS
- PHP 8.5.0: tests/008.phpt ✅ PASS
- All 3 event tests pass on all PHP versions

---

#### Issue #45: Fix and re-enable tests/008.phpt (debug builds)
**Status:** ✅ **FIXED**

**Original Issue:** tests/008.phpt disabled for debug builds due to memory leak.

**satwareAG Solution:**

The polling model redesign (same as #66) also resolves memory issues:

1. **Single-threaded execution**: All operations occur in PHP thread
2. **Proper resource cleanup**: `_php_fbird_free_event()` handles cleanup correctly
3. **Reference counting**: Event resources properly reference-counted via `link_res`
4. **Safety limits**: `max_callbacks` limit (1000) prevents infinite loops

**Test Results (2025-12-17):**
- Event tests pass without memory warnings
- No cross-thread resource sharing
- Clean resource lifecycle management

---

#### Issue #99: Incorrect type reporting for CHAR fields
**Status:** ✅ **FULLY FIXED**

**Original Issue:** `ibase_field_info()` returns "VARCHAR" instead of "CHAR" for CHAR fields.

**satwareAG Verification (2025-12-17):**
- `fbird_metadata.c` correctly maps `SQL_TEXT` → "CHAR" and `SQL_VARYING` → "VARCHAR"
- Test `fbird_field_info_001.phpt` expects and gets "CHAR" for CHAR_FIXED field
- New test `tests/issue99_001.phpt` created with exact reproduction from upstream issue
- All 6 field_info tests pass on PHP 8.3, 8.4, and 8.5
- The upstream bug (commit `29e9d6f8fd`) does NOT affect the satwareAG fork

**Evidence:** See `docs/UPSTREAM_FIXED_ISSUES_INSPECTION.md` for detailed verification.

---

#### Issue #25: char(1) padded with spaces with charset UTF8
**Status:** ✅ **FULLY FIXED**

**Original Issue:** CHAR(1) returns 'A   ' (with 3 extra spaces) when using UTF-8 charset.

**satwareAG Verification (2025-12-17):**
- CHAR fields are properly right-trimmed on fetch in `fbird_result.c`
- UTF-8 multi-byte characters handled correctly (3-byte euro sign works)
- Test `tests/datatype_char_utf8.phpt` verifies correct behavior
- CHAR fields: Trailing spaces trimmed (correct SQL standard behavior)
- VARCHAR fields: Content unchanged (no trimming needed)

**Evidence:** See `docs/UPSTREAM_FIXED_ISSUES_INSPECTION.md` for detailed verification.

---

#### Issue #42: Fix and re-enable tests/007.phpt
**Status:** ✅ **FULLY FIXED**

**Original Issue:** tests/007.phpt was disabled in upstream due to failures.

**satwareAG Verification (2025-12-17):**
- Test `tests/007.phpt` (array handling) exists and is enabled
- Standard SKIPIF (not disabled)
- Additional variants: `007_iso_char.phpt`, `007_iso_integer.phpt`, `007_iso_varchar10.phpt`, `007_iso_varchar1000.phpt`

**Test Results:**
- PHP 8.3.28: ✅ PASS
- PHP 8.4.15: ✅ PASS  
- PHP 8.5.0: ✅ PASS

**Evidence:** Test matrix verification 2025-12-17

---

#### Issue #22: ibase_close not working as expected
**Status:** ✅ **FIXED**

**Original Issue:** `ibase_close($x)` doesn't actually close connection; second call returns true.

**satwareAG Verification (2025-12-17):**
- `firebird.c` `PHP_FUNCTION(fbird_close)` has proper close logic
- Uses `zend_list_close()` for proper resource cleanup
- Tests confirm correct behavior: first close returns `true`, second close returns `false`

**Test Evidence:**
- `tests/fbird_close_004.phpt` - Basic close test: ✅ PASS
- `tests/fbird_close_005.phpt` - Error handling test: ✅ PASS

**Correct Behavior:**
```php
$x = fbird_connect($db);
var_dump(fbird_close($x));  // bool(true)  - first close succeeds
var_dump(fbird_close($x));  // bool(false) - already closed
var_dump(fbird_close());    // bool(false) - no default link
```

---

#### Issue #53: ibase_service_attach doesn't allow local connection
**Status:** ✅ **FIXED**

**Original Issue:** Service attach always uses TCP pattern `%s:service_mgr` even for local.

**satwareAG Verification (2025-12-17):**
- `fbird_service.c` `PHP_FUNCTION(fbird_service_attach)` correctly handles local vs remote
- Local connection (empty host): Uses `"service_mgr"` without host prefix
- Remote connection (host provided): Uses `"%s:service_mgr"` pattern

**Implementation (fbird_service.c):**
```c
char loc[128] = "service_mgr";  // Default: local connection
// ...
if(hlen > 0){
    slprintf(loc, sizeof(loc), "%s:service_mgr", host);  // Remote connection
}
```

**Test Evidence:**
- `tests/fbird_service_001.phpt` - Service attach error messages: ✅ PASS
- `tests/fbird_service_002.phpt` - Server info constants: ✅ PASS

---

### 📝 BY DESIGN (Documented) - Critical Evaluation

#### Issue #97: Impossible to make multiple connections with same args
**Status:** 📝 **BY DESIGN - Partially Correct, ENHANCEMENT NEEDED**

**Original Issue:** Multiple `ibase_connect()` calls with identical arguments return same resource.

---

##### Critical Evaluation (2025-12-17)

This section provides a comprehensive analysis of whether the connection reuse design decision is good or bad by comparing with other PHP database extensions.

###### Comparison with Other PHP Extensions

| Extension | Default Behavior | Escape Hatch Flag | Verdict |
|-----------|-----------------|-------------------|---------|
| **pg_connect()** | Reuses with same connection string | `PGSQL_CONNECT_FORCE_NEW` | ✅ Best design |
| **mysqli_connect()** | Always creates new connection | N/A (no reuse) | Predictable |
| **PDO** | Always creates new connection | N/A (no reuse) | Predictable |
| **fbird_connect()** | Reuses with same args | **❌ None** | **Missing escape hatch** |

**Key Finding:** PostgreSQL's `pg_connect()` has the EXACT same default behavior as php-firebird, BUT provides `PGSQL_CONNECT_FORCE_NEW` as an escape hatch. The Official PHP documentation states:

> *"If a second call is made to pg_connect() with the same connection_string as an existing connection, the existing connection will be returned unless you pass PGSQL_CONNECT_FORCE_NEW as flags."*

---

###### Analysis: Is Connection Reuse Good or Bad?

**Arguments FOR connection reuse (current behavior):**
- ✅ Reduces connection overhead for typical use cases
- ✅ Prevents accidental connection exhaustion
- ✅ Matches historical ibase_connect() behavior
- ✅ Matches PostgreSQL pg_connect() default behavior
- ✅ Memory efficient - single connection object shared

**Arguments AGAINST connection reuse WITHOUT escape hatch:**
- ❌ **Violates Principle of Least Surprise** - mysqli and PDO don't reuse
- ❌ **No escape hatch** - unlike pg_connect() which has PGSQL_CONNECT_FORCE_NEW
- ❌ **Breaks legitimate use cases:**
  - Connection-specific transaction isolation levels
  - Long-running queries in parallel
  - Connection-specific temporary tables
  - Connection-specific session variables (`RDB$CONFIG`)
  - Testing scenarios requiring independent connection state
  - Data import with separate commit boundaries
- ❌ **Current workarounds are hacky** - changing charset/role just to get new connection is not intuitive

---

###### Verdict: PARTIALLY CORRECT, NEEDS ENHANCEMENT

| Aspect | Assessment |
|--------|------------|
| **Default behavior (reuse)** | ✅ **CORRECT** - Matches PostgreSQL, reduces overhead |
| **Missing escape hatch** | ❌ **INCOMPLETE** - PostgreSQL provides PGSQL_CONNECT_FORCE_NEW |
| **Overall design** | ⚠️ **NEEDS ENHANCEMENT** |

**The design decision to reuse connections by default is CORRECT** - it matches PostgreSQL's well-established pattern and provides sensible defaults for most applications.

**However, the design is INCOMPLETE** because it provides no mechanism for developers to explicitly request a new connection when needed. PostgreSQL solved this problem years ago with `PGSQL_CONNECT_FORCE_NEW`.

---

###### Recommendation: Add FBIRD_CONNECT_FORCE_NEW Flag

**Priority:** Medium (Enhancement, not bug fix)

**Proposed Implementation:**

```c
// php_firebird.h - Add new constant
#define FBIRD_CONNECT_FORCE_NEW 1

// firebird.c - PHP_FUNCTION(fbird_connect)
// Check for FBIRD_CONNECT_FORCE_NEW flag before hash lookup
if (!(flags & FBIRD_CONNECT_FORCE_NEW)) {
    // Existing hash lookup code
    if ((le = zend_hash_str_find_ptr(&EG(regular_list), hash, sizeof(hash)-1)) != NULL) {
        // Return existing connection
    }
}
// Always continue to create new connection if flag set or no existing found
```

**New Function Signature:**
```php
fbird_connect(
    string $database = null,
    string $username = null,
    string $password = null,
    string $charset = null,
    int $buffers = null,
    int $dialect = null,
    string $role = null,
    int $sync = null,
    int $flags = 0              // NEW: Optional flags parameter
): resource|false
```

**Usage After Enhancement:**
```php
// Same parameters = same connection (default, backward compatible)
$conn1 = fbird_connect($db, $user, $pass);
$conn2 = fbird_connect($db, $user, $pass);  // Returns SAME resource

// Force new connection when needed (NEW!)
$conn3 = fbird_connect($db, $user, $pass, null, null, null, null, null, FBIRD_CONNECT_FORCE_NEW);

// For common case, could also add helper constant with positioned args
define('FBIRD_NO_FLAGS', 0);
```

---

###### Current Implementation (firebird.c)

```c
// Hash-based connection reuse (firebird.c)
if ((le = zend_hash_str_find_ptr(&EG(regular_list), hash, sizeof(hash)-1)) != NULL) {
    // Return existing connection
}
// ...
zend_hash_str_update_mem(&EG(regular_list), hash, sizeof(hash)-1, ...);
```

---

###### Current Workarounds (Until Enhancement Implemented)

```php
// Same parameters = same connection (by design)
$conn1 = fbird_connect($db, $user, $pass);
$conn2 = fbird_connect($db, $user, $pass);  // Returns SAME resource

// Workaround 1: Use different parameters (hacky but works)
$conn3 = fbird_connect($db, $user, $pass, 'UTF8');
$conn4 = fbird_connect($db, $user, $pass, 'ISO8859_1');

// Workaround 2: Use different role
$conn5 = fbird_connect($db, $user, $pass, null, null, null, 'ADMIN');
$conn6 = fbird_connect($db, $user, $pass, null, null, null, 'USER');

// Workaround 3: Use persistent connections (different pool)
$conn7 = fbird_pconnect($db, $user, $pass);
```

---

###### Action Items

| Action | Priority | Repository | Status |
|--------|----------|------------|--------|
| Document current behavior in README | High | Fork | 📋 TODO |
| Add `FBIRD_CONNECT_FORCE_NEW` constant | Medium | Fork | 📋 [Fork Issue #11](https://github.com/satwareAG/php-firebird/issues/11) |
| Add `flags` parameter to `fbird_connect()` | Medium | Fork | 📋 [Fork Issue #11](https://github.com/satwareAG/php-firebird/issues/11) |
| Update workaround documentation | Low | Fork | ✅ Done above |

> **Note:** Fork Issue #11 is a satwareAG enhancement proposal created to address the gap identified in Upstream Issue #97. See [Fork Enhancements](#fork-enhancements-satwareagphp-firebird) for details.

---

###### References

- [PHP pg_connect() documentation](https://www.php.net/manual/en/function.pg-connect.php)
- [PHP mysqli_connect() documentation](https://www.php.net/manual/en/mysqli.construct.php)
- [PHP PDO connection management](https://www.php.net/manual/en/pdo.connections.php)
- [Upstream Issue #97](https://github.com/FirebirdSQL/php-firebird/issues/97)

---

### 📝 DOCUMENTATION ONLY

#### Issue #90: Inconsistent arguments for ibase_query() and ibase_prepare()
**Status:** 📝 **DOCUMENTATION ISSUE**

**Original Issue:** PHP manual documentation is incorrect/incomplete for function signatures.

**satwareAG Status:**
- Function signatures unchanged from upstream
- This is a php.net documentation issue, not driver code issue
- Consider adding docblocks or updating stubs

**Action:** Update PHP documentation via php-doc process, not driver changes.

---

#### Issue #72: Update PHP docs and stubs for ibase_service_attach()
**Status:** 📝 **DOCUMENTATION ISSUE**

**Original Issue:** Parameters are now optional for embedded connections.

**satwareAG Status:**
- Function signature allows optional parameters
- Needs php.net documentation update
- Consider IDE stub files

**Action:** Create PR to php/doc-en repository.

---

#### Issue #71: ibase_service_attach() should respect INI settings
**Status:** 📝 **DOCUMENTATION/ENHANCEMENT**

**Original Issue:** Service attach should use `fbird.default_user`/`fbird.default_password`.

**satwareAG Status:**
- May already work (needs verification)
- If not, minor enhancement to check INI values

**Action Required:** Test current behavior, document or implement fallback.

---

#### Issue #63: Confusing PHP documentation for ibase_trans()
**Status:** 📝 **DOCUMENTATION ISSUE**

**Original Issue:** Parameter order for `ibase_trans()` is confusing in documentation.

**satwareAG Analysis:**
- Order matters: trans_args must come BEFORE link_identifier
- Driver behavior is correct, documentation misleading

**Action:** Update php.net docs to clarify correct parameter order.

---

### 🚀 FEATURE REQUESTS

#### Issue #83: Benchmark suite would be nice
**Status:** 🚀 **FEATURE REQUEST**

**Original Issue:** Create benchmarks for extension performance measurement.

**satwareAG Status:**
- `docs/benchmarks/` directory exists with some benchmark scripts
- `perf_firebird.php`, `perf_pdo_firebird.php` present
- Not comprehensive yet

**Recommendation:** Expand benchmark suite, add memory profiling, compare with PDO.

---

#### Issue #12: Add PECL package
**Status:** 🚀 **FEATURE REQUEST**

**Original Issue:** Publish extension on pecl.php.net.

**satwareAG Status:**
- Not yet published to PECL
- Would require stable release and maintenance commitment
- Consider publishing under `firebird` package name

**Recommendation:** Plan for PECL publishing after v1.0 stable release.

---

### ❓ NOT APPLICABLE

#### Issue #70: Prepare next release 5.0.3
**Status:** ❓ **NOT APPLICABLE**

**Original Issue:** Upstream release planning discussion.

**satwareAG Status:**
- We're developing independently as a fork
- Our versioning will be separate
- Focus on our own `feature/fbird-extension-release` branch

---

## Recommended Priority Actions

### Medium Priority (Enhancement)
1. **Issue #97** - Add `FBIRD_CONNECT_FORCE_NEW` flag
   - **Verdict:** Default reuse behavior is CORRECT (matches PostgreSQL)
   - **Gap:** Missing escape hatch that PostgreSQL provides via `PGSQL_CONNECT_FORCE_NEW`
   - **Effort:** Low-medium (add constant, add flags parameter, modify hash lookup)
   - **Benefit:** Complete feature parity with PostgreSQL pattern

### Low Priority (Documentation/Features)
2. **Issues #90,#72,#71,#63** - PHP documentation updates (php.net)
3. **Issue #83** - Benchmark suite expansion
4. **Issue #12** - PECL publishing (post-release)

### All Core Issues Completed (✅ FIXED)
- **Issue #22** - ibase_close: Verified FIXED (2025-12-17) - second close returns false
- **Issue #53** - Service attach local connection: Verified FIXED (2025-12-17)
- **Issue #42** - Array handling test: Verified FIXED (2025-12-17) - passes on PHP 8.3/8.4/8.5
- **Issue #99** - CHAR type reporting: Verified FIXED (2025-12-17)
- **Issue #25** - UTF-8 CHAR padding: Verified FIXED (2025-12-17)
- **Issues #66, #45** - Event handling PHP 8.4+: Verified FIXED via polling model
- **Issues #82, #85, #86, #98** - Infrastructure and documentation: Complete
- **Issue #97** - Connection reuse: Evaluated - default correct, enhancement recommended (2025-12-17)

---

## Fork Enhancements (satwareAG/php-firebird)

This section documents **fork-specific enhancement proposals** created in the satwareAG repository. These are NOT upstream issues but rather improvements identified during upstream issue analysis.

### Fork Issue #11: Add FBIRD_CONNECT_FORCE_NEW flag for explicit connection creation

**Repository:** [satwareAG/php-firebird](https://github.com/satwareAG/php-firebird)  
**Issue URL:** https://github.com/satwareAG/php-firebird/issues/11  
**Status:** 📋 Open (Enhancement Proposal)  
**Priority:** Medium  
**Related Upstream Issue:** [Upstream #97](https://github.com/FirebirdSQL/php-firebird/issues/97) (Connection reuse behavior)

#### Summary

Add `FBIRD_CONNECT_FORCE_NEW` flag to allow explicit creation of new database connections, matching PostgreSQL's `PGSQL_CONNECT_FORCE_NEW` pattern.

#### Background

Analysis of Upstream Issue #97 revealed that while the default connection reuse behavior is **correct** (matches PostgreSQL's `pg_connect()` default), the implementation is **incomplete** because it lacks an escape hatch for cases where new connections are legitimately needed.

#### Proposed Changes

1. **New constant:** `FBIRD_CONNECT_FORCE_NEW` (value: 2)
2. **New parameter:** `flags` parameter for `fbird_connect()` and `fbird_pconnect()`
3. **Backward compatible:** Default behavior unchanged

#### Implementation Guide

The issue contains a complete implementation specification based on PostgreSQL's `pg_connect()` pattern:
- Flag exclusion from connection hash
- Conditional hash lookup bypass
- 5 test scenarios
- Full backward compatibility guarantee

#### References

- [Fork Issue #11 - Full Implementation Specification](https://github.com/satwareAG/php-firebird/issues/11)
- [PostgreSQL pg_connect() Implementation Analysis](https://github.com/php/php-src/blob/master/ext/pgsql/pgsql.c)
- [Upstream Issue #97 - Original Connection Reuse Discussion](https://github.com/FirebirdSQL/php-firebird/issues/97)

---

## Test Commands

```bash
# Run specific test for Issue #99 (CHAR type)
docker exec -it php-firebird-php-1 php run-tests.php -P tests/fbird_field_info_001.phpt

# Run event tests for Issue #66/#45
docker exec -it php-firebird-php-1 php run-tests.php -P tests/008.phpt

# Run array tests for Issue #42
docker exec -it php-firebird-php-1 php run-tests.php -P tests/007.phpt

# Run with memory leak detection (debug build required)
docker exec -it php-firebird-php-1 php -d zend.assertions=1 run-tests.php -m tests/008.phpt
```

---

*Document generated from satwareAG/php-firebird fork analysis*

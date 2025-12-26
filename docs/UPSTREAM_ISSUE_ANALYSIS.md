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
| ✅ **FIXED** in satwareAG fork | 12 | #82, #85, #86, #98, #99, #25, #66, #45, #42, #22, #53, #71 |
| ⚠️ **BY DESIGN** (enhancement done) | 1 | #97 → Fork #11 ✅ IMPLEMENTED |
| 📝 **DOCUMENTATION ONLY** | 3 | #90, #72, #63 |
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

#### Issue #98: Modern date/time/timestamp format parsing
**Status:** ✅ **FULLY FIXED**

**Original Issue:** Replace deprecated `strptime()` with PHP date parsing facilities for `fbird.timestampformat`, `fbird.dateformat`, `fbird.timeformat`.

**satwareAG Implementation (Verified 2025-12-17):**

The satwareAG fork completely replaced `strptime()` with a cross-platform `sscanf()`-based parsing implementation:

**Design (fbird_datetime.c):**
- `fbird_parse_date()` - Parses ISO 8601, European (DD.MM.YYYY), and US (MM/DD/YYYY) formats
- `fbird_parse_time()` - Parses HH:MM:SS.FFFF format with timezone extraction
- `fbird_parse_timestamp()` - Combined date/time parsing with auto-format detection
- All functions use `sscanf()` instead of non-portable `strptime()`

**Usage (fbird_query_bind.c, lines 703-779):**
```c
// Binding PHP date strings to Firebird datatypes
parsed = fbird_parse_date(Z_STRVAL_P(b_var), &dt);      // SQL_TYPE_DATE
parsed = fbird_parse_time(Z_STRVAL_P(b_var), &dt);      // SQL_TYPE_TIME  
parsed = fbird_parse_timestamp(Z_STRVAL_P(b_var), &dt); // SQL_TIMESTAMP
```

**Cross-Platform Benefits:**
- No `strptime()` dependency (missing on Windows, inconsistent on platforms)
- Works on Linux (glibc/musl), Windows, and macOS
- Auto-detects date format based on separator characters (-, ., /)
- Validates parsed components before accepting

**Test Results (2025-12-17):**
- `tests/time_003.phpt` - PASS
- `tests/time_004.phpt` - PASS
- `tests/timezone_001.phpt` - PASS
- `tests/timezone_002.phpt` - PASS
- `tests/timezone_003.phpt` - PASS

**Note:** The implementation uses C-level `sscanf()` parsing rather than PHP DateTime API, which is more efficient for extension code and avoids the overhead of calling PHP functions from C.

**Evidence:** `grep -rn strptime` shows only documentation comments, no actual function calls.

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
| Document current behavior in README | High | Fork | ✅ Done (README.md "Connection Behavior" section) |
| Add `FBIRD_CONNECT_FORCE_NEW` constant | Medium | Fork | 📋 [Fork Issue #11](https://github.com/satwareAG/php-firebird/issues/11) |
| Add `flags` parameter to `fbird_connect()` | Medium | Fork | 📋 [Fork Issue #11](https://github.com/satwareAG/php-firebird/issues/11) |
| Update workaround documentation | Low | Fork | ✅ Done (README.md includes workarounds) |

> **Note:** Fork Issue #11 is a satwareAG enhancement proposal created to address the gap identified in Upstream Issue #97. See [Fork Enhancements](#fork-enhancements-satwareagphp-firebird) for details.

---

###### References

- [PHP pg_connect() documentation](https://www.php.net/manual/en/function.pg-connect.php)
- [PHP mysqli_connect() documentation](https://www.php.net/manual/en/mysqli.construct.php)
- [PHP PDO connection management](https://www.php.net/manual/en/pdo.connections.php)
- [Upstream Issue #97](https://github.com/FirebirdSQL/php-firebird/issues/97)

---

### 📝 DOCUMENTATION ONLY - Deep Analysis

The following 3 issues require updates to PHP.net documentation (https://www.php.net/manual/) and IDE stubs,
not code changes to the extension. Below is a comprehensive analysis of each issue with specific documentation
errors identified and proposed corrections.

---

#### Issue #90: Inconsistent arguments for ibase_query() and ibase_prepare()

**Status:** 📝 **DOCUMENTATION ISSUE**  
**Upstream URL:** https://github.com/FirebirdSQL/php-firebird/issues/90  
**Reporter:** mlazdans (Oct 29, 2025)  
**Priority:** High (affects daily usage)

##### Problem Summary

The PHP documentation has **3 distinct errors** for these functions:

1. **`ibase_query()`** - Documentation shows only `$link_identifier`, but it actually accepts `$trans_identifier` as well
2. **`ibase_prepare()`** - Documentation has `string $trans` when it should be `resource $trans_identifier` (TYPE ERROR)
3. **`ibase_prepare()`** - Cannot use `ibase_prepare($trans_id, ...)` pattern that works with `ibase_query()`

##### Current PHP.net Documentation

**ibase_query()** (php.net/manual/en/function.ibase-query.php):
```php
ibase_query(resource $link_identifier = ?, string $query, int $bind_args = ?): resource
```

**ibase_prepare()** (php.net/manual/en/function.ibase-prepare.php):
```php
ibase_prepare(string $query): resource
ibase_prepare(resource $link_identifier, string $query): resource
ibase_prepare(resource $link_identifier, string $trans, string $query): resource  // BUG: $trans is string, not resource!
```

##### Actual Implementation (satwareAG fork - fbird_query_exec.c)

Both functions use **flexible argument parsing** that detects resource types at runtime:

```c
// fbird_query() implementation (lines 999-1080)
// - Can accept link OR transaction resource
// - Auto-detects via zend_fetch_resource_ex()
// - Supports patterns: ($query), ($link, $query), ($trans, $query), ($link, $trans, $query), ($trans, $link, $query)

// fbird_prepare() implementation (lines 1191-1280)
// - Similar flexible parsing
// - If transaction provided without link, infers link from transaction
```

##### Correct Documentation

**fbird_query() / ibase_query()** should document:
```php
fbird_query(string $query [, mixed ...$bind_args]): resource|int|bool
fbird_query(resource $link_identifier, string $query [, mixed ...$bind_args]): resource|int|bool
fbird_query(resource $trans_identifier, string $query [, mixed ...$bind_args]): resource|int|bool
fbird_query(resource $link_identifier, resource $trans_identifier, string $query [, mixed ...$bind_args]): resource|int|bool
```

**fbird_prepare() / ibase_prepare()** should document:
```php
fbird_prepare(string $query): resource|false
fbird_prepare(resource $link_identifier, string $query): resource|false
fbird_prepare(resource $trans_identifier, string $query): resource|false  // NEW - currently undocumented!
fbird_prepare(resource $link_identifier, resource $trans_identifier, string $query): resource|false
```

##### Proposed php-doc XML Corrections

**File:** `reference/ibase/functions/ibase-query.xml`

```xml
<!-- Add multiple methodsynopsis blocks showing all valid signatures -->
<methodsynopsis>
 <type class="union"><type>resource</type><type>int</type><type>bool</type></type>
 <methodname>ibase_query</methodname>
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
 <methodparam rep="repeat" choice="opt"><type>mixed</type><parameter>bind_args</parameter></methodparam>
</methodsynopsis>

<methodsynopsis>
 <type class="union"><type>resource</type><type>int</type><type>bool</type></type>
 <methodname>ibase_query</methodname>
 <methodparam><type>resource</type><parameter>link_identifier</parameter></methodparam>
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
 <methodparam rep="repeat" choice="opt"><type>mixed</type><parameter>bind_args</parameter></methodparam>
</methodsynopsis>

<methodsynopsis>
 <type class="union"><type>resource</type><type>int</type><type>bool</type></type>
 <methodname>ibase_query</methodname>
 <methodparam><type>resource</type><parameter>trans_identifier</parameter></methodparam>
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
 <methodparam rep="repeat" choice="opt"><type>mixed</type><parameter>bind_args</parameter></methodparam>
</methodsynopsis>
```

**File:** `reference/ibase/functions/ibase-prepare.xml`

```xml
<!-- FIX: Change string $trans to resource $trans_identifier -->
<methodsynopsis>
 <type class="union"><type>resource</type><type>false</type></type>
 <methodname>ibase_prepare</methodname>
 <methodparam><type>resource</type><parameter>link_identifier</parameter></methodparam>
 <methodparam><type>resource</type><parameter>trans_identifier</parameter></methodparam>  <!-- WAS: string $trans -->
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
</methodsynopsis>

<!-- ADD: Missing signature for transaction-only -->
<methodsynopsis>
 <type class="union"><type>resource</type><type>false</type></type>
 <methodname>ibase_prepare</methodname>
 <methodparam><type>resource</type><parameter>trans_identifier</parameter></methodparam>
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
</methodsynopsis>
```

##### IDE Stub Corrections (phpstorm-stubs)

**File:** `interbase/interbase.php` (JetBrains/phpstorm-stubs repository)

```php
/**
 * Execute a query on an InterBase/Firebird database
 * @link https://php.net/manual/en/function.ibase-query.php
 * @param resource|string $link_or_trans_or_query Link, transaction, or query string
 * @param string|resource $query_or_trans_or_bind Query string, transaction, or bind arg
 * @param mixed ...$bind_args Optional bind arguments
 * @return resource|int|bool Result set, affected rows, or false on error
 */
function ibase_query($link_or_trans_or_query, $query_or_trans_or_bind = null, ...$bind_args) {}

/**
 * Prepare a query for later binding and execution
 * @link https://php.net/manual/en/function.ibase-prepare.php
 * @param resource|string $link_or_trans_or_query Link, transaction, or query string
 * @param resource|string $trans_or_query Transaction or query string
 * @param string $query Query string (when both link and trans provided)
 * @return resource|false Prepared query handle or false on error
 */
function ibase_prepare($link_or_trans_or_query, $trans_or_query = null, $query = null) {}
```

##### Action Items

| Action | Target | Priority |
|--------|--------|----------|
| Fix `string $trans` → `resource $trans_identifier` | php/doc-en | High |
| Add transaction-only signatures to ibase_query() | php/doc-en | High |
| Add transaction-only signature to ibase_prepare() | php/doc-en | High |
| Update phpstorm-stubs | JetBrains/phpstorm-stubs | Medium |

---

#### Issue #72: Update PHP docs and stubs for ibase_service_attach()

**Status:** 📝 **DOCUMENTATION ISSUE**  
**Upstream URL:** https://github.com/FirebirdSQL/php-firebird/issues/72  
**Reporter:** mlazdans (Oct 13, 2025)  
**Assignee:** MartinKoeditz  
**Priority:** Medium (affects embedded Firebird usage)

##### Problem Summary

The PHP documentation shows all 3 parameters as **required** when they are actually **all optional**
for embedded Firebird connections.

##### Current PHP.net Documentation

**ibase_service_attach()** (php.net/manual/en/function.ibase-service-attach.php):
```php
ibase_service_attach(string $host, string $dba_username, string $dba_password): resource|false
```

All parameters shown as **required** (no `= ?` optional marker).

##### Actual Implementation (satwareAG fork - fbird_service.c)

```c
// fbird_service.c, PHP_FUNCTION(fbird_service_attach)
if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS(), "|s!s!s!",
        &host, &hlen, &user, &ulen, &pass, &plen)) {
    RETURN_FALSE;
}

// "|" prefix means ALL following parameters are optional
// "s!" means nullable string

/* Fall back to INI defaults if user/password not provided (Issue #71) */
if (ulen == 0) {
    char *ini_user = INI_STR("fbird.default_user");
    if (ini_user && *ini_user) {
        user = ini_user;
        ulen = strlen(ini_user);
    }
}
```

##### Correct Documentation

**fbird_service_attach() / ibase_service_attach()** should document:
```php
/**
 * Connect to the service manager
 * 
 * @param string|null $host The name or IP address of the database host.
 *                          If omitted or null, connects to local embedded service manager.
 * @param string|null $dba_username The DBA username. If omitted, falls back to
 *                                  fbird.default_user INI directive.
 * @param string|null $dba_password The DBA password. If omitted, falls back to
 *                                  fbird.default_password INI directive.
 * @return resource|false Service handle on success, false on failure
 */
fbird_service_attach(?string $host = null, ?string $dba_username = null, ?string $dba_password = null): resource|false
```

##### Use Cases Enabled by Optional Parameters

```php
// Remote connection with explicit credentials
$svc = fbird_service_attach('10.1.1.199', 'SYSDBA', 'masterkey');

// Local embedded connection with INI defaults
// php.ini: fbird.default_user=SYSDBA, fbird.default_password=masterkey
$svc = fbird_service_attach();  // Uses local service_mgr + INI defaults

// Local embedded with explicit credentials
$svc = fbird_service_attach(null, 'SYSDBA', 'masterkey');

// Remote with INI defaults for credentials
$svc = fbird_service_attach('10.1.1.199');  // Uses INI defaults for user/pass
```

##### Proposed php-doc XML Correction

**File:** `reference/ibase/functions/ibase-service-attach.xml`

```xml
<methodsynopsis>
 <type class="union"><type>resource</type><type>false</type></type>
 <methodname>ibase_service_attach</methodname>
 <methodparam choice="opt"><type class="union"><type>string</type><type>null</type></type><parameter>host</parameter><initializer>&null;</initializer></methodparam>
 <methodparam choice="opt"><type class="union"><type>string</type><type>null</type></type><parameter>dba_username</parameter><initializer>&null;</initializer></methodparam>
 <methodparam choice="opt"><type class="union"><type>string</type><type>null</type></type><parameter>dba_password</parameter><initializer>&null;</initializer></methodparam>
</methodsynopsis>

<!-- Add to description -->
<para>
 If <parameter>host</parameter> is omitted or &null;, connects to the local
 embedded service manager (<literal>service_mgr</literal>).
</para>
<para>
 If <parameter>dba_username</parameter> or <parameter>dba_password</parameter>
 are omitted or &null;, the function falls back to the
 <link linkend="ini.fbird.default-user">fbird.default_user</link> and
 <link linkend="ini.fbird.default-password">fbird.default_password</link>
 INI directives respectively.
</para>
```

##### IDE Stub Correction

```php
/**
 * Connect to the service manager
 * @link https://php.net/manual/en/function.ibase-service-attach.php
 * @param string|null $host [optional] Host name/IP. Null for local embedded.
 * @param string|null $dba_username [optional] Username. Falls back to fbird.default_user INI.
 * @param string|null $dba_password [optional] Password. Falls back to fbird.default_password INI.
 * @return resource|false Service handle or false on error
 */
function ibase_service_attach(?string $host = null, ?string $dba_username = null, ?string $dba_password = null) {}
```

##### Action Items

| Action | Target | Priority |
|--------|--------|----------|
| Change all params to optional | php/doc-en | High |
| Add INI fallback documentation | php/doc-en | High |
| Add local connection example | php/doc-en | Medium |
| Update phpstorm-stubs | JetBrains/phpstorm-stubs | Medium |

---

#### Issue #71: ibase_service_attach() should respect INI settings
**Status:** ✅ **FIXED** (2025-12-17)

**Original Issue:** Service attach should use `fbird.default_user`/`fbird.default_password` when user/password parameters are not provided.

**satwareAG Verification (2025-12-17):**
- **Confirmed bug:** The original `fbird_service_attach()` did NOT fall back to INI defaults
- **Fix applied:** Added INI_STR() fallback in `fbird_service.c` to match `fbird_connect()` behavior
- **Test created:** `tests/fbird_service_ini_defaults.phpt` verifies INI fallback works

**Implementation (fbird_service.c):**
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

**Behavior After Fix:**
```php
// php.ini: fbird.default_user=SYSDBA, fbird.default_password=masterkey

// Now works - uses INI defaults (Issue #71 fix)
$svc = fbird_service_attach('localhost');  // Uses SYSDBA/masterkey from INI

// Still works - explicit credentials override INI
$svc = fbird_service_attach('localhost', 'CUSTOM_USER', 'custom_pass');
```

---

#### Issue #63: Confusing PHP documentation for ibase_trans()

**Status:** 📝 **DOCUMENTATION ISSUE**  
**Upstream URL:** https://github.com/FirebirdSQL/php-firebird/issues/63  
**Reporter:** mlazdans (Mar 2, 2025)  
**Assignee:** MartinKoeditz (PR created, not yet merged)  
**Priority:** High (silent failure leads to data corruption risk)

##### Problem Summary

The PHP documentation shows **two conflicting signatures** implying parameter order doesn't matter,
but in reality **order is critical**: `trans_args` MUST come BEFORE `link_identifier`, otherwise
the transaction arguments are **silently ignored**.

##### Current PHP.net Documentation

**ibase_trans()** (php.net/manual/en/function.ibase-trans.php):
```php
ibase_trans(int $trans_args = ?, resource $link_identifier = ?): resource
ibase_trans(resource $link_identifier = ?, int $trans_args = ?): resource  // MISLEADING!
```

The documentation shows two signatures suggesting either order works.

##### Actual Behavior (CRITICAL)

```php
// ❌ WRONG - trans_args SILENTLY IGNORED (appears to work but uses default transaction!)
$tr = ibase_trans($db, IBASE_READ);
ibase_query($tr, "INSERT INTO TEST_TABLE (COL) VALUES(123)");  // INSERTS despite IBASE_READ!
ibase_commit($tr);  // Commits! Data modified despite "read-only" transaction

// ✅ CORRECT - trans_args actually applied
$tr = ibase_trans(IBASE_READ, $db);
ibase_query($tr, "INSERT INTO TEST_TABLE (COL) VALUES(123)");  // Fails as expected!
```

**This is dangerous:** Code appears to work but silently uses default transaction isolation,
which could lead to data corruption in concurrent scenarios.

##### Actual Implementation (satwareAG fork - firebird.c)

```c
// PHP_FUNCTION(fbird_trans) implementation
// The function processes arguments in ORDER, and non-resource arguments
// specify modifiers for the NEXT resource argument that follows.

/* enumerate all the arguments: assume every non-resource argument
   specifies modifiers for the link ids that follow it */
for (i = 0; i < argn; ++i) {
    if (Z_TYPE(args[i]) == IS_RESOURCE) {
        // This is a connection link
        // Apply the PREVIOUSLY collected trans_args to this link
        memcpy(&tpb[TPB_MAX_SIZE * link_cnt], last_tpb, TPB_MAX_SIZE);
        // ...
    } else {
        // This is trans_args - populate last_tpb for the NEXT link
        convert_to_long_ex(&args[i]);
        trans_argl = Z_LVAL(args[i]);
        _php_fbird_populate_trans(trans_argl, trans_timeout, last_tpb, &tpb_len);
    }
}
```

The key insight: **Non-resource arguments set modifiers for the FOLLOWING resource arguments.**

##### Correct Usage Patterns

```php
// Pattern 1: Single connection with transaction args
// trans_args BEFORE link
$tr = fbird_trans(FBIRD_READ | FBIRD_CONSISTENCY, $db);

// Pattern 2: Default transaction (no trans_args)
$tr = fbird_trans($db);

// Pattern 3: Multi-database transaction with different args per connection
// args1 applies to db1, args2 applies to db2
$tr = fbird_trans(FBIRD_WRITE, $db1, FBIRD_READ, $db2);

// Pattern 4: Same args for all connections
// FBIRD_READ applies to BOTH db1 and db2
$tr = fbird_trans(FBIRD_READ, $db1, $db2);

// Pattern 5: Complex multi-database with WAIT + LOCK_TIMEOUT (only for first)
$tr = fbird_trans(FBIRD_READ | FBIRD_WAIT | FBIRD_LOCK_TIMEOUT, 5, $db1, FBIRD_WRITE, $db2);
```

##### Correct Documentation

**fbird_trans() / ibase_trans()** should document:

```php
/**
 * Begin a transaction
 * 
 * Arguments are processed in ORDER. Non-resource arguments (trans_args)
 * specify modifiers for the NEXT resource argument (link_identifier) that follows.
 * 
 * @param int $trans_args [optional] Transaction parameters for the FOLLOWING link.
 *                        Combination of FBIRD_READ, FBIRD_WRITE, FBIRD_COMMITTED,
 *                        FBIRD_CONSISTENCY, FBIRD_CONCURRENCY, FBIRD_REC_VERSION,
 *                        FBIRD_REC_NO_VERSION, FBIRD_WAIT, FBIRD_NOWAIT.
 * @param resource $link_identifier Database link. If omitted, uses default.
 * @param mixed ...$more_args_and_links Additional trans_args and links for multi-database transactions.
 * @return resource|false Transaction handle or false on error
 * 
 * IMPORTANT: trans_args must come BEFORE the link_identifier they apply to!
 * 
 * Examples:
 *   fbird_trans(FBIRD_READ, $db)           - Read-only transaction
 *   fbird_trans($db)                       - Default transaction  
 *   fbird_trans(FBIRD_WRITE, $db1, $db2)   - Same args for both
 *   fbird_trans(FBIRD_READ, $db1, FBIRD_WRITE, $db2) - Different args
 */
fbird_trans(int $trans_args = FBIRD_DEFAULT, resource ...$links_and_args): resource|false
```

##### Proposed php-doc XML Correction

**File:** `reference/ibase/functions/ibase-trans.xml`

```xml
<!-- Remove the misleading second signature -->
<!-- Replace with a single variadic signature showing correct order -->
<methodsynopsis>
 <type class="union"><type>resource</type><type>false</type></type>
 <methodname>ibase_trans</methodname>
 <methodparam choice="opt"><type>int</type><parameter>trans_args</parameter><initializer>IBASE_DEFAULT</initializer></methodparam>
 <methodparam choice="opt" rep="repeat"><type class="union"><type>resource</type><type>int</type></type><parameter>links_and_args</parameter></methodparam>
</methodsynopsis>

<!-- Add clear warning about parameter order -->
<warning>
 <para>
  <emphasis>Parameter order matters!</emphasis> Transaction arguments
  (<parameter>trans_args</parameter>) must appear <emphasis>before</emphasis>
  the <parameter>link_identifier</parameter> they apply to. If a link
  identifier appears before transaction arguments, the arguments will be
  <emphasis>silently ignored</emphasis> and the default transaction
  parameters will be used instead.
 </para>
 <para>
  <emphasis>Correct:</emphasis> <literal>ibase_trans(IBASE_READ, $db)</literal>
 </para>
 <para>
  <emphasis>Incorrect:</emphasis> <literal>ibase_trans($db, IBASE_READ)</literal>
  (IBASE_READ is ignored!)
 </para>
</warning>

<!-- Update description to explain multi-database transactions -->
<para>
 For multi-database transactions, arguments are processed left to right.
 Each transaction argument applies to all subsequent link identifiers until
 another transaction argument is encountered.
</para>
<example>
 <title>Multi-database transaction with different isolation levels</title>
 <programlisting role="php">
<![CDATA[
<?php
// db1 gets IBASE_READ, db2 gets IBASE_WRITE
$tr = ibase_trans(IBASE_READ, $db1, IBASE_WRITE, $db2);

// Both db1 and db2 get IBASE_READ
$tr = ibase_trans(IBASE_READ, $db1, $db2);
?>
]]>
 </programlisting>
</example>
```

##### IDE Stub Correction

```php
/**
 * Begin a transaction
 * 
 * IMPORTANT: Transaction arguments must come BEFORE the link they apply to!
 * ibase_trans(IBASE_READ, $db) is correct.
 * ibase_trans($db, IBASE_READ) silently ignores IBASE_READ!
 * 
 * @link https://php.net/manual/en/function.ibase-trans.php
 * @param int $trans_args [optional] Transaction parameters (IBASE_READ, IBASE_WRITE, etc.)
 * @param resource ...$links_and_args Links and optional additional trans_args for multi-db
 * @return resource|false Transaction handle or false on error
 */
function ibase_trans(int $trans_args = IBASE_DEFAULT, ...$links_and_args) {}
```

##### Upstream Status

MartinKoeditz created a PR for PHP docs (April 2025) but it has **not been merged** as of November 2025.
The upstream extension also added a check to warn about incorrect order (commit `eba7584`).

##### Action Items

| Action | Target | Priority | Status |
|--------|--------|----------|--------|
| Follow up on MartinKoeditz's pending PHP docs PR | php/doc-en | High | PR exists, not merged |
| Add prominent warning about parameter order | php/doc-en | High | In pending PR |
| Remove misleading second signature | php/doc-en | High | In pending PR |
| Add multi-database transaction examples | php/doc-en | Medium | In pending PR |
| Update phpstorm-stubs with correct signature | JetBrains/phpstorm-stubs | Medium | TODO |

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
2. **Issues #90,#72,#63** - PHP documentation updates (php.net)
3. **Issue #83** - Benchmark suite expansion
4. **Issue #12** - PECL publishing (post-release)

### All Core Issues Completed (✅ FIXED)
- **Issue #22** - ibase_close: Verified FIXED (2025-12-17) - second close returns false
- **Issue #53** - Service attach local connection: Verified FIXED (2025-12-17)
- **Issue #42** - Array handling test: Verified FIXED (2025-12-17) - passes on PHP 8.3/8.4/8.5
- **Issue #71** - Service attach INI defaults: FIXED (2025-12-17) - added INI_STR() fallback
- **Issue #99** - CHAR type reporting: Verified FIXED (2025-12-17)
- **Issue #25** - UTF-8 CHAR padding: Verified FIXED (2025-12-17)
- **Issues #66, #45** - Event handling PHP 8.4+: Verified FIXED via polling model
- **Issues #82, #85, #86, #98** - Infrastructure and documentation: Complete
- **Issue #97** - Connection reuse: ENHANCEMENT DONE → Fork Issue #11 IMPLEMENTED (2025-12-17)

---

## Fork Enhancements (satwareAG/php-firebird)

This section documents **fork-specific enhancement proposals** created in the satwareAG repository. These are NOT upstream issues but rather improvements identified during upstream issue analysis.

### Fork Issue #11: Add FBIRD_CONNECT_FORCE_NEW flag for explicit connection creation

**Repository:** [satwareAG/php-firebird](https://github.com/satwareAG/php-firebird)  
**Issue URL:** https://github.com/satwareAG/php-firebird/issues/11  
**Status:** ✅ **IMPLEMENTED** (2025-12-17)  
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

## Appendix A: Cross-Extension Parameter Best Practices Analysis

**Research Date:** 2025-12-17  
**Purpose:** Ensure php-firebird parameter implementations follow industry best practices by comparing with PostgreSQL, MySQLi, and PDO extensions.

---

### A.1 Executive Summary

Deep analysis of PHP database extensions reveals that **php-firebird's flexible parameter handling is actually MORE powerful than other extensions**, not a bug. The ability to pass transaction resources directly to `fbird_query()` and `fbird_prepare()` is a unique feature that other extensions lack.

| Feature | PostgreSQL | MySQLi | PDO | php-firebird |
|---------|------------|--------|-----|--------------|
| **Connection reuse** | PGSQL_CONNECT_FORCE_NEW | Always new | Always new | FBIRD_CONNECT_FORCE_NEW ✅ |
| **Transaction in query()** | ❌ No | ❌ No | ❌ No | ✅ **Yes** (unique!) |
| **Transaction in prepare()** | ❌ No | ❌ No | ❌ No | ✅ **Yes** (unique!) |
| **Default connection** | ✅ Yes (last) | ❌ No | N/A (OOP) | ✅ Yes (last) |
| **Optional params** | ✅ Partial | ❌ Required | N/A | ✅ Full |
| **INI fallback** | ❌ No | ❌ No | ❌ No | ✅ Yes |

**Key Finding:** php-firebird's transaction-aware query/prepare functions are a **feature advantage**, not a documentation error. The documentation should highlight this capability.

---

### A.2 PostgreSQL (pgsql) Extension Analysis

#### A.2.1 pg_connect() - Connection Management

```php
pg_connect(string $connection_string, int $flags = 0): PgSql\Connection|false
```

**Key Features:**
- Connection reuse by default (same connection string = same connection)
- `PGSQL_CONNECT_FORCE_NEW` (value: 1) forces new connection
- `PGSQL_CONNECT_ASYNC` (value: 2) for asynchronous connections
- Connection string contains all parameters (host, port, dbname, user, password)

**Best Practice Applied to php-firebird:**
- ✅ Already implemented: `FBIRD_CONNECT_FORCE_NEW` flag (Fork Issue #11)
- ✅ Connection reuse is correct default behavior
- ✅ Pattern match: PostgreSQL is the gold standard for connection handling

#### A.2.2 pg_query() - Query Execution

```php
pg_query(PgSql\Connection $connection = ?, string $query): PgSql\Result|false
```

**Key Features:**
- Connection is OPTIONAL (uses last connection if omitted)
- Single query parameter (no transaction support)
- Returns result or false

**Comparison with fbird_query():**
| Aspect | pg_query() | fbird_query() |
|--------|------------|---------------|
| Connection optional | ✅ Yes | ✅ Yes |
| Transaction support | ❌ No | ✅ **Yes** (unique feature!) |
| Bind args | ❌ No (use pg_query_params) | ✅ Yes (variadic) |

**Verdict:** fbird_query() is MORE flexible than pg_query() by supporting both connection AND transaction resources.

#### A.2.3 pg_prepare() - Prepared Statements

```php
pg_prepare(PgSql\Connection $connection = ?, string $stmtname, string $query): PgSql\Result|false
```

**Key Features:**
- Named statements (unique per connection)
- NO transaction parameter
- Requires separate pg_execute() call

**Comparison with fbird_prepare():**
| Aspect | pg_prepare() | fbird_prepare() |
|--------|--------------|-----------------|
| Connection optional | ✅ Yes | ✅ Yes |
| Transaction support | ❌ No | ✅ **Yes** (unique feature!) |
| Statement naming | Required | Not supported |
| Execution | pg_execute() | fbird_execute() |

**Verdict:** fbird_prepare() has unique transaction awareness that pg_prepare() lacks.

---

### A.3 MySQLi Extension Analysis

#### A.3.1 mysqli_connect() - Connection Management

```php
mysqli_connect(
    ?string $hostname = null,
    ?string $username = null,
    ?string $password = null,
    ?string $database = null,
    ?int $port = null,
    ?string $socket = null
): mysqli|false
```

**Key Features:**
- Always creates new connection (no reuse)
- No force-new flag needed (always new)
- All parameters optional

**Comparison with fbird_connect():**
| Aspect | mysqli_connect() | fbird_connect() |
|--------|------------------|-----------------|
| Connection reuse | ❌ Never | ✅ By default |
| Force new flag | N/A | ✅ FBIRD_CONNECT_FORCE_NEW |
| Optional params | ✅ All | ✅ All |
| INI fallback | ❌ No | ✅ Yes (fbird.default_user/password) |

**Verdict:** fbird_connect() follows PostgreSQL's superior connection reuse pattern.

#### A.3.2 mysqli_query() - Query Execution

```php
mysqli_query(mysqli $mysql, string $query, int $result_mode = MYSQLI_STORE_RESULT): mysqli_result|bool
```

**Key Features:**
- Connection is REQUIRED (no default)
- No transaction support
- Result mode parameter for memory optimization

**Comparison with fbird_query():**
| Aspect | mysqli_query() | fbird_query() |
|--------|----------------|---------------|
| Connection required | ✅ Yes | ❌ Optional |
| Transaction support | ❌ No | ✅ **Yes** (unique feature!) |
| Result mode | ✅ Yes | ❌ No |
| Bind args | ❌ No | ✅ Yes (variadic) |

#### A.3.3 mysqli_prepare() - Prepared Statements

```php
mysqli_prepare(mysqli $mysql, string $query): mysqli_stmt|false
```

**Key Features:**
- Connection is REQUIRED
- NO transaction parameter
- Returns statement object for binding

**Comparison:**
| Aspect | mysqli_prepare() | fbird_prepare() |
|--------|------------------|-----------------|
| Connection required | ✅ Yes | ❌ Optional |
| Transaction support | ❌ No | ✅ **Yes** (unique feature!) |

#### A.3.4 MySQLi Transaction Management

```php
mysqli_begin_transaction(mysqli $mysql, int $flags = 0, ?string $name = null): bool
mysqli_commit(mysqli $mysql, int $flags = 0, ?string $name = null): bool
mysqli_rollback(mysqli $mysql, int $flags = 0, ?string $name = null): bool
```

**Key Features:**
- Explicit transaction start required
- Transaction flags: `MYSQLI_TRANS_START_READ_ONLY`, `MYSQLI_TRANS_START_READ_WRITE`
- Named savepoints supported

**Comparison with fbird_trans():**
| Aspect | MySQLi | php-firebird |
|--------|--------|--------------|
| Explicit start | ✅ begin_transaction() | ✅ fbird_trans() |
| Transaction flags | ✅ Yes (flags param) | ✅ Yes (trans_args) |
| Read-only mode | ✅ MYSQLI_TRANS_START_READ_ONLY | ✅ FBIRD_READ |
| Multi-database | ❌ No | ✅ **Yes** (unique!) |
| Savepoints | ✅ mysqli_savepoint() | ✅ fbird_savepoint() (SQL) |

**Verdict:** fbird_trans() supports multi-database transactions that MySQLi cannot.

---

### A.4 PDO Extension Analysis

#### A.4.1 PDO Connection Management

```php
new PDO(string $dsn, ?string $username = null, ?string $password = null, ?array $options = null)
```

**Key Features:**
- Always creates new connection
- DSN-based connection string
- Driver-specific options

**PDO_FIREBIRD DSN Examples:**
```php
// Local database
$pdo = new PDO('firebird:dbname=/path/to/DATABASE.FDB', 'SYSDBA', 'masterkey');

// Remote with port
$pdo = new PDO('firebird:dbname=hostname/port:/path/to/DATABASE.FDB', 'user', 'pass');

// With dialect
$pdo = new PDO('firebird:dbname=localhost:/data/test.fdb;charset=utf-8;dialect=1');
```

#### A.4.2 PDO::prepare() - Prepared Statements

```php
PDO::prepare(string $query, array $options = []): PDOStatement|false
```

**Key Features:**
- No connection parameter (uses current object)
- No transaction parameter
- Options for cursor type, etc.

**Comparison:**
| Aspect | PDO::prepare() | fbird_prepare() |
|--------|----------------|-----------------|
| Connection | Implicit (object) | Optional (resource/default) |
| Transaction | ❌ No | ✅ **Yes** (unique feature!) |
| Bind style | Named (:name) or ? | ✅ Both supported |

#### A.4.3 PDO Transaction Management

```php
PDO::beginTransaction(): bool
PDO::commit(): bool
PDO::rollBack(): bool
PDO::inTransaction(): bool
```

**Key Features:**
- Automatic autocommit management
- No transaction flags or isolation level in beginTransaction()
- Driver-specific attributes for isolation level

**PDO_FIREBIRD Transaction Constants:**
```php
Pdo\Firebird::TRANSACTION_ISOLATION_LEVEL  // Attribute key
Pdo\Firebird::READ_COMMITTED               // Default isolation
Pdo\Firebird::REPEATABLE_READ              // Snapshot
Pdo\Firebird::SERIALIZABLE                 // Snapshot table stability
Pdo\Firebird::WRITABLE_TRANSACTION         // READ WRITE vs READ ONLY
```

**Usage:**
```php
$pdo->setAttribute(Pdo\Firebird::TRANSACTION_ISOLATION_LEVEL, Pdo\Firebird::SERIALIZABLE);
$pdo->setAttribute(Pdo\Firebird::WRITABLE_TRANSACTION, false);  // READ ONLY
$pdo->beginTransaction();
```

**Comparison with fbird_trans():**
| Aspect | PDO_FIREBIRD | fbird_trans() |
|--------|--------------|---------------|
| Isolation levels | ✅ Via setAttribute() | ✅ Via trans_args |
| Read-only mode | ✅ WRITABLE_TRANSACTION | ✅ FBIRD_READ |
| Lock timeout | ❌ No | ✅ FBIRD_LOCK_TIMEOUT |
| Multi-database | ❌ No | ✅ **Yes** (unique!) |
| Table reservations | ❌ No | ✅ **Yes** (unique!) |

**Verdict:** fbird_trans() is significantly more powerful than PDO's transaction API.

---

### A.5 Key Findings and Best Practice Recommendations

#### A.5.1 fbird_trans() - Variadic Parameter Semantics

**Current Behavior (CORRECT):**
```php
// Pattern: trans_args BEFORE link they apply to
$tr = fbird_trans(FBIRD_READ | FBIRD_CONSISTENCY, $db);

// Multi-database: different args per connection (UNIQUE FEATURE!)
$tr = fbird_trans(FBIRD_READ, $db1, FBIRD_WRITE, $db2);
```

**Best Practice Verdict:** ✅ **DESIGN IS CORRECT**

php-firebird's variadic fbird_trans() is MORE powerful than any other PHP database extension:
- PostgreSQL: No multi-database transactions
- MySQLi: No multi-database transactions
- PDO: No multi-database transactions, limited isolation control

**Documentation Requirement:**
The parameter order semantics MUST be clearly documented because no other extension works this way. The current documentation issue (#63) correctly identifies this need.

#### A.5.2 fbird_query() / fbird_prepare() - Transaction Support

**Current Behavior (UNIQUE FEATURE):**
```php
// Standard: link-based query
$result = fbird_query($link, "SELECT * FROM test");

// Advanced: transaction-based query (UNIQUE!)
$tr = fbird_trans(FBIRD_READ, $link);
$result = fbird_query($tr, "SELECT * FROM test");  // Query uses specific transaction!
fbird_commit($tr);
```

**Best Practice Verdict:** ✅ **THIS IS A FEATURE, NOT A BUG**

No other PHP database extension supports this pattern:
- PostgreSQL pg_query(): NO transaction parameter
- MySQLi mysqli_query(): NO transaction parameter
- PDO PDOStatement: NO transaction parameter

**Why This Matters:**
1. **Fine-grained control:** Execute specific queries in specific transactions
2. **Isolation flexibility:** Read queries in READ ONLY transaction, writes in READ WRITE
3. **Multi-database transactions:** Query can span databases in same transaction
4. **Performance:** Avoid starting new transactions for simple reads

**Documentation Requirement:**
This feature should be **HIGHLIGHTED** as an advantage, not hidden:
```php
// PHP official docs should show:
// Signature 1: Basic query
fbird_query(string $query [, mixed ...$bind_args]): resource|int|bool

// Signature 2: Connection-specific query  
fbird_query(resource $link_identifier, string $query [, mixed ...$bind_args]): resource|int|bool

// Signature 3: Transaction-specific query (UNIQUE FEATURE!)
fbird_query(resource $trans_identifier, string $query [, mixed ...$bind_args]): resource|int|bool

// Signature 4: Full control
fbird_query(resource $link_identifier, resource $trans_identifier, string $query [, mixed ...$bind_args]): resource|int|bool
```

#### A.5.3 fbird_service_attach() - Optional Parameters with INI Fallback

**Current Behavior (CORRECT):**
```php
// All parameters optional with INI fallback
fbird_service_attach(?string $host = null, ?string $user = null, ?string $pass = null)

// Falls back to:
// - fbird.default_user INI directive
// - fbird.default_password INI directive
```

**Best Practice Comparison:**
| Extension | Optional Credentials | INI Fallback |
|-----------|---------------------|--------------|
| PostgreSQL | ❌ In connection string | ✅ .pgpass file |
| MySQLi | ✅ All optional | ❌ No INI fallback |
| PDO | ✅ Optional | ❌ No INI fallback |
| php-firebird | ✅ All optional | ✅ **INI fallback** |

**Verdict:** ✅ **DESIGN IS CORRECT AND SUPERIOR**

php-firebird's INI fallback pattern is more convenient than other extensions. PostgreSQL achieves similar convenience via `.pgpass` file, but php-firebird's INI approach is more PHP-native.

**Documentation Requirement:**
The optional nature and INI fallback MUST be documented clearly (Issue #72).

---

### A.6 Documentation Priority Matrix

Based on this analysis, here is the prioritized documentation update plan:

| Issue | Function | Problem | Documentation Change | Priority |
|-------|----------|---------|---------------------|----------|
| **#63** | fbird_trans() | Parameter order misleading | Remove second signature, add WARNING about order | **CRITICAL** |
| **#90** | fbird_query/prepare() | Missing transaction signatures | ADD transaction signatures as FEATURE | **HIGH** |
| **#90** | fbird_prepare() | Wrong type: `string $trans` | FIX to `resource $trans_identifier` | **HIGH** |
| **#72** | fbird_service_attach() | All params shown as required | Change to optional, document INI fallback | **MEDIUM** |

---

### A.7 Proposed PHP.net Documentation Updates

#### A.7.1 fbird_query() Documentation (Issue #90)

**Current php.net (INCOMPLETE):**
```xml
<methodsynopsis>
 <type class="union"><type>resource</type><type>int</type><type>bool</type></type>
 <methodname>ibase_query</methodname>
 <methodparam choice="opt"><type>resource</type><parameter>link_identifier</parameter></methodparam>
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
 <methodparam choice="opt" rep="repeat"><type>mixed</type><parameter>bind_args</parameter></methodparam>
</methodsynopsis>
```

**Proposed (COMPLETE):**
```xml
<!-- Signature 1: Query only (uses default link) -->
<methodsynopsis>
 <type class="union"><type>resource</type><type>int</type><type>bool</type></type>
 <methodname>ibase_query</methodname>
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
 <methodparam choice="opt" rep="repeat"><type>mixed</type><parameter>bind_args</parameter></methodparam>
</methodsynopsis>

<!-- Signature 2: Connection-specific -->
<methodsynopsis>
 <type class="union"><type>resource</type><type>int</type><type>bool</type></type>
 <methodname>ibase_query</methodname>
 <methodparam><type>resource</type><parameter>link_identifier</parameter></methodparam>
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
 <methodparam choice="opt" rep="repeat"><type>mixed</type><parameter>bind_args</parameter></methodparam>
</methodsynopsis>

<!-- Signature 3: Transaction-specific (UNIQUE FEATURE) -->
<methodsynopsis>
 <type class="union"><type>resource</type><type>int</type><type>bool</type></type>
 <methodname>ibase_query</methodname>
 <methodparam><type>resource</type><parameter>trans_identifier</parameter></methodparam>
 <methodparam><type>string</type><parameter>query</parameter></methodparam>
 <methodparam choice="opt" rep="repeat"><type>mixed</type><parameter>bind_args</parameter></methodparam>
</methodsynopsis>

<!-- Description addition -->
<note>
 <title>Transaction-Specific Queries</title>
 <para>
  Unlike other PHP database extensions (mysqli, pgsql, PDO), ibase_query() can accept
  a <parameter>trans_identifier</parameter> resource to execute the query within a
  specific transaction. The function auto-detects whether the resource is a connection
  or transaction based on the resource type.
 </para>
</note>
```

#### A.7.2 fbird_trans() Documentation (Issue #63)

**Proposed WARNING block:**
```xml
<warning>
 <title>Parameter Order is Critical</title>
 <para>
  Transaction arguments (<parameter>trans_args</parameter>) must appear 
  <emphasis>before</emphasis> the <parameter>link_identifier</parameter> they apply to.
  Arguments are processed left-to-right, with each integer argument setting modifiers
  for all subsequent connection resources.
 </para>
 <para>
  <emphasis>Correct:</emphasis> <literal>ibase_trans(IBASE_READ, $db)</literal>
 </para>
 <para>
  <emphasis>Incorrect:</emphasis> <literal>ibase_trans($db, IBASE_READ)</literal>
  — The <constant>IBASE_READ</constant> flag is silently ignored!
 </para>
</warning>

<example>
 <title>Multi-database transaction with different isolation levels</title>
 <para>
  This unique feature of Firebird/InterBase is not available in other PHP database
  extensions like mysqli, pgsql, or PDO.
 </para>
 <programlisting role="php">
<![CDATA[
<?php
// db1 gets IBASE_READ (read-only), db2 gets IBASE_WRITE
$tr = ibase_trans(IBASE_READ, $db1, IBASE_WRITE, $db2);

// Same isolation for both connections
$tr = ibase_trans(IBASE_COMMITTED | IBASE_REC_VERSION, $db1, $db2);

// Lock timeout only for first connection
$tr = ibase_trans(IBASE_WRITE | IBASE_WAIT | IBASE_LOCK_TIMEOUT, 10, $db1, IBASE_WRITE, $db2);
?>
]]>
 </programlisting>
</example>
```

---

### A.8 Conclusion

This deep research confirms that **php-firebird's parameter handling represents best practices** and in many cases **exceeds the capabilities of other PHP database extensions**:

1. **fbird_connect():** Matches PostgreSQL's superior connection reuse pattern with FORCE_NEW escape hatch ✅
2. **fbird_trans():** UNIQUE multi-database transaction support that no other extension offers ✅
3. **fbird_query()/fbird_prepare():** UNIQUE transaction-aware queries not available in mysqli/pgsql/PDO ✅
4. **fbird_service_attach():** Superior convenience with optional params + INI fallback ✅

The only issues are **documentation clarity**, not implementation bugs. The PHP.net documentation should be updated to:
1. Highlight these unique features as advantages
2. Clarify the critical parameter ordering for fbird_trans()
3. Fix the type error in fbird_prepare() documentation
4. Mark service_attach parameters as optional

---

*Appendix generated 2025-12-17 as part of satwareAG/php-firebird deep parameter analysis*

---

*Document generated from satwareAG/php-firebird fork analysis*

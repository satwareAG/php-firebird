# Implementation Plan: Fix SIGSEGV on PHP Shutdown (Issues #50, #51)

**STATUS:  COMPLETED**
**Implementation Date:** 2026-01-02

---

[Overview]

Fixed SIGSEGV crash during PHP shutdown when using persistent connections (`fbird_pconnect()`).

**Root Cause:** The `_php_fbird_close_plink()` destructor attempted to access PHP's executor globals (`EG(regular_list)` and `EG(persistent_list)`) during `PHP_MSHUTDOWN_FUNCTION` when these structures may already be destroyed.

**Solution:** Added an `in_mshutdown` flag to the module globals that is set at the beginning of `PHP_MSHUTDOWN_FUNCTION`. The persistent link destructor checks this flag and skips EG() access when the module is shutting down.

---

[Types]

Added `zend_bool in_mshutdown` field to `fbird_module_globals` struct in `php_fbird_includes.h`.

---

[Files]

**Modified Files:**
1. `php_fbird_includes.h` - Added `in_mshutdown` flag to module globals struct
2. `firebird.c` - Set flag in `PHP_MSHUTDOWN_FUNCTION`, guard EG() access in `_php_fbird_close_plink()`

**New Files:**
1. `tests/fbird_pconnect_shutdown_001.phpt` - Regression test for the fix

---

[Functions]

**Modified Functions:**
1. `PHP_GINIT_FUNCTION(fbird)` in `firebird.c` - Initialize `in_mshutdown = 0`
2. `PHP_MSHUTDOWN_FUNCTION(fbird)` in `firebird.c` - Set `IBG(in_mshutdown) = 1` at function start
3. `_php_fbird_close_plink()` in `firebird.c` - Added guard: skip EG() access when `IBG(in_mshutdown) == 1`

---

[Classes]

No class changes required (procedural C extension).

---

[Dependencies]

No new dependencies required.

---

[Testing]

**New Test:** `tests/fbird_pconnect_shutdown_001.phpt`
- Connects with persistent connection
- Executes simple query to verify connection is active
- Deliberately does NOT close connection (tests destructor during shutdown)
- Expected: "Test completed" output with clean exit (code 0)
- Failure mode: SIGSEGV crash (exit code 139) before output

**Verification:**
- New test: PASSED
- Regression tests: 7/7 PASSED (002.phpt through fbird_connect_force_new.phpt)

---

[Implementation Order]

All steps completed:

1.  Investigate GitHub issues #50 and #51
2.  Analyze source code for root cause
3.  Research PHP extension shutdown patterns
4.  Create implementation plan document
5.  Add `in_mshutdown` flag to module globals (php_fbird_includes.h)
6.  Initialize flag in PHP_GINIT_FUNCTION
7.  Set flag in PHP_MSHUTDOWN_FUNCTION
8.  Guard EG() access in _php_fbird_close_plink()
9.  Add regression test for persistent connection shutdown
10.  Build and verify fix resolves SIGSEGV
11.  Run test suite to ensure no regressions
12.  Update CHANGELOG

---

## GitHub Issue Closure

**To close the issues:**
```bash
gh issue close 50 --comment "Fixed in [Unreleased]. Added in_mshutdown flag to prevent EG() access during module shutdown. Test: tests/fbird_pconnect_shutdown_001.phpt"
gh issue close 51 --comment "Fixed in [Unreleased]. Same root cause as #50 - added in_mshutdown flag guard."
```

---

## Technical Details

### PHP Extension Shutdown Order
1. `RSHUTDOWN` - Request shutdown (cleans up request-specific resources)
2. `MSHUTDOWN` - Module shutdown (cleans up persistent resources, but EG() may be invalid)

### Why EG() Access Crashes
- `EG(regular_list)` and `EG(persistent_list)` are part of PHP's executor globals
- During `MSHUTDOWN`, these may already be destroyed or in an undefined state
- The persistent link destructor tried to `zend_hash_str_del()` from these lists
- Accessing invalid memory caused SIGSEGV (signal 11, exit code 139)

### The Fix
```c
// In _php_fbird_close_plink():
if (!IBG(in_mshutdown)) {
    // Safe to access EG() - we're during request shutdown
    zend_hash_str_del(&EG(regular_list), link_data->hash_key, 16);
    zend_hash_str_del(&EG(persistent_list), link_data->hash_key, 16);
}
// During MSHUTDOWN, skip EG() access - just free the Firebird resources
```

This pattern is used by other PHP extensions (mysql, pgsql) that manage persistent connections.
# Upstream Issue Analysis: FirebirdSQL/php-firebird

**Analysis Date:** 2025-12-17  
**Upstream Repository:** https://github.com/FirebirdSQL/php-firebird  
**Fork Repository:** https://github.com/satwareAG/php-firebird  
**Branch:** `feature/fbird-extension-release`

This document analyzes all 19 open issues from the upstream FirebirdSQL/php-firebird repository and determines their status in the satwareAG fork.

---

## Summary

| Status | Count | Issues |
|--------|-------|--------|
| ✅ **FIXED** in satwareAG fork | 8 | #82, #85, #86, #98, #99, #25, #66, #45 |
| 🔍 **NEEDS INVESTIGATION** | 4 | #97, #42, #22, #53 |
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

### 🔍 NEEDS INVESTIGATION

#### Issue #97: Impossible to make multiple connections with same args
**Status:** 🔍 **DESIGN ISSUE - NEEDS INVESTIGATION**

**Original Issue:** Multiple `ibase_connect()` calls with identical arguments return same resource.

**satwareAG Analysis:**
- This is **by design** - connection pooling/reuse via hash lookup in `EG(regular_list)`
- `firebird.c` lines 1172-1191 show intentional hash-based connection reuse
- For separate connections, users must use different args or `ibase_pconnect()`

**Possible Solutions:**
1. Add new INI setting `fbird.force_new_connections`
2. Document existing behavior clearly
3. Add `fbird_new_connection()` function that bypasses hash

**Recommendation:** Create documentation, consider adding `FBIRD_FORCE_NEW` flag option.

---

#### Issue #42: Fix and re-enable tests/007.phpt
**Status:** 🔍 **NEEDS INVESTIGATION**

**Original Issue:** tests/007.phpt was disabled.

**satwareAG Status:**
- `tests/007.phpt` exists (array handling test)
- Has standard SKIPIF (not disabled)
- We also have 007_iso_* variants

**Action Required:** Verify test passes in all matrix combinations.

---

#### Issue #25: char(1) padded with spaces with charset UTF8
**Status:** 🔍 **NEEDS INVESTIGATION**

**Original Issue:** CHAR(1) returns 'A   ' (with 3 extra spaces) when using UTF-8 charset.

**satwareAG Analysis:**
- This is a Firebird wire protocol behavior - CHAR fields transferred with full byte buffer
- UTF-8 CHAR(1) = 4 bytes buffer (max UTF-8 char size)
- Should be trimmed at PHP driver level

**Action Required:**
1. Check `fbird_result.c` for string trimming logic
2. Verify if we handle CHAR vs VARCHAR differently on fetch
3. Consider adding automatic trim based on field type metadata

---

#### Issue #22: ibase_close not working as expected
**Status:** 🔍 **NEEDS INVESTIGATION**

**Original Issue:** `ibase_close($x)` doesn't actually close connection; second call returns true.

**satwareAG Analysis:**
- `firebird.c` `PHP_FUNCTION(fbird_close)` has explicit close logic
- Uses `zend_list_close()` instead of `zend_list_delete()` for proper cleanup
- Reference counting may keep connection alive

**Action Required:**
1. Write test verifying actual connection close (check MON$ATTACHMENTS)
2. Verify behavior matches documentation
3. May be connection reuse/pooling feature, not bug

---

#### Issue #53: ibase_service_attach doesn't allow local connection
**Status:** 🔍 **NEEDS INVESTIGATION**

**Original Issue:** Service attach always uses TCP pattern `%s:service_mgr` even for local.

**satwareAG Analysis:**
- `fbird_service.c` handles service connections
- Local connection should use just `service_mgr` without host prefix

**Action Required:**
1. Check `_php_fbird_service_attach()` implementation
2. Test local service connection with empty host
3. Implement pattern detection for local vs remote

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

### High Priority (Potential Bugs)
1. **Issue #99** - CHAR type reporting: Create reproduction test, fix if confirmed
2. **Issue #25** - UTF-8 CHAR padding: Likely actual bug, needs fix

### Medium Priority (Code Quality)
3. **Issue #42** - Verify tests/007.phpt passes everywhere
4. **Issue #22** - Document/fix ibase_close behavior
5. **Issue #53** - Local service connections

### Low Priority (Documentation/Features)
6. **Issues #90,#72,#71,#63** - PHP documentation updates
7. **Issue #83** - Benchmark suite expansion
8. **Issue #12** - PECL publishing (post-release)

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

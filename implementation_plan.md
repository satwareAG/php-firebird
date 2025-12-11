# Implementation Plan: Fix All Clang-Tidy Warnings for 2025 Code Quality

[Overview]
Fix all 46 clang-tidy warnings across 7 C source files to achieve maximum code quality for PHP Firebird extension in late 2025.

This implementation addresses static analysis warnings reported by clang-tidy during QA checks, categorized into:
- **Critical bugs**: Uninitialized values, garbage values (6 warnings)
- **Code safety**: Missing default cases, type conversions, pointer sign mismatches (16 warnings)
- **Code readability**: else-after-return, nested conditionals, redundant casts (18 warnings)
- **Loop safety**: Too-small loop variables (4 warnings)
- **Type safety**: Always-true conditions on array addresses (9 warnings)

The fixes ensure the extension compiles cleanly with zero warnings while maintaining full backward compatibility with all 95 passing tests.

[Types]
No new types required - all fixes are refactoring existing code for better safety and clarity.

**Type Changes:**
- `unsigned short i` loop counters → `int i` or `uint32_t i` where comparing against `int` bounds
- Several implicit casts need explicit type annotations

[Files]
Seven C source files require modifications to eliminate clang-tidy warnings.

**Files to Modify:**

1. **fbird_blobs.c** - 4 warnings
   - Line 193-209: Remove else-after-return in `_php_fbird_string_to_quad()`
   - Line 213-221: Remove else-after-return in `_php_fbird_quad_to_string()`, fix void* cast
   - Line 315: Add default case to switch in `_php_fbird_blob_info()`

2. **fbird_events.c** - 3 warnings
   - Line 67, 70: Fix pointer sign mismatch in `isc_free()` calls
   - Line 202: Fix loop variable type narrower than upper bound

3. **fbird_metadata.c** - 15 warnings
   - Lines 128, 138, 143, 378, 380, 408, 410: Remove always-true array address checks
   - Lines 161, 181: Add default cases to switches
   - Lines 380, 410: Refactor nested conditional operators
   - Line 421: Remove else-after-continue
   - Line 496: Remove else-after-return

4. **fbird_query_exec.c** - 18 warnings
   - Line 153: Remove tautological comparison
   - Lines 357, 365, 398, 418, 1085: Add default cases to switches
   - Line 491: Remove extraneous parentheses
   - Line 566: Remove redundant cast
   - Lines 787, 792: Remove always-true array address checks
   - Lines 1041, 1047, 1064: Fix uninitialized value bugs (CRITICAL)
   - Line 1044: Fix loop variable type
   - Line 1667: Remove else-after-return
   - Line 1981: Remove else-after-break

5. **fbird_result.c** - 2 warnings
   - Line 130: Add default case to switch
   - Line 375: Fix loop variable type

6. **fbird_service.c** - 3 warnings
   - Line 344: Remove else-after-return
   - Lines 375, 402: Add default cases to switches

7. **fbird_udf.c** - 5 warnings (CRITICAL FILE)
   - Line 152: Initialize `result` variable
   - Line 157: Fix constant conversion truncation (65536 → 0)
   - Line 161: Fix switch-on-boolean
   - Line 182: Fix garbage value in comparison
   - Line 235: Fix pointer sign in ZVAL_STRINGL

[Functions]
Functions to modify for warning fixes.

**Modified Functions:**

**fbird_blobs.c:**
- `_php_fbird_string_to_quad()` - Refactor to eliminate else-after-return
- `_php_fbird_quad_to_string()` - Refactor to eliminate else-after-return and fix void* cast
- `_php_fbird_blob_info()` - Add default case to switch

**fbird_events.c:**
- `_php_fbird_free_event()` - Cast unsigned char* to char* for isc_free()
- `PHP_FUNCTION(fbird_set_event_handler)` - Fix loop variable type from `unsigned short` to `uint32_t`

**fbird_metadata.c:**
- `_php_fbird_build_field_info()` - Remove unnecessary array address checks, add default cases
- `_php_fbird_populate_index_for_param_fields()` - Refactor nested conditionals, remove else-after-continue
- `_php_fbird_infer_returning_prefix()` - Remove else-after-return

**fbird_query_exec.c:**
- `_php_fbird_alloc_xsqlda()` - Remove tautological comparison
- `_php_fbird_bind_params()` - Add default cases to 4 switches, remove extra parentheses, fix array checks, remove else-after-break
- `_php_fbird_bind_array()` - Fix loop variable, add default case, fix uninitialized value bugs (CRITICAL)
- `_php_fbird_exec()` - Remove else-after-return, remove redundant cast
- `PHP_FUNCTION(fbird_query)` - Already correct (context)

**fbird_result.c:**
- `_php_fbird_fetch_hash()` - Add default case to switch
- `_php_fbird_fetch_array()` - Fix loop variable type

**fbird_service.c:**
- `PHP_FUNCTION(fbird_backup)` or `PHP_FUNCTION(fbird_restore)` - Remove else-after-return
- Service info switch statements - Add default cases

**fbird_udf.c:**
- `fbird_udf_evaluate()` - Initialize result=0, fix min() macro overflow, fix switch-bool, fix pointer sign

[Classes]
No classes - this is C code, not C++.

N/A for C extension code.

[Dependencies]
No new dependencies required.

All fixes use standard C patterns and existing Firebird/PHP APIs.

[Testing]
Verify all 95 tests continue to pass after each file modification.

**Testing Strategy:**

1. **After each file fix:**
   ```bash
   scripts/host/qa_full.sh
   ```
   Verify:
   - Build succeeds
   - Clang-tidy warnings reduced
   - All 95 tests pass

2. **Final verification:**
   - Run `clang-tidy` standalone to confirm 0 warnings
   - Run full test suite across PHP 8.1-8.5
   - Run cppcheck to ensure no new issues

3. **Regression testing focus:**
   - BLOB operations (fbird_blobs.c changes)
   - Event handling (fbird_events.c changes)
   - Array binding (fbird_query_exec.c critical fixes)
   - Result fetching (fbird_result.c, fbird_metadata.c)
   - Service API (fbird_service.c)
   - UDF support (fbird_udf.c)

[Implementation Order]
Fix files in order of criticality, starting with critical bugs then moving to code quality.

1. **fbird_udf.c** (CRITICAL - 5 warnings)
   - Fix uninitialized `result` variable
   - Fix 65536 constant truncation to 0
   - Fix switch-on-boolean to if statement
   - Fix pointer sign in ZVAL_STRINGL
   - Verify: Build + fbird_udf tests (if any)

2. **fbird_query_exec.c** (CRITICAL - 18 warnings)
   - Fix uninitialized value bugs in `_php_fbird_bind_array()` (lines 1041, 1047, 1064)
   - Add default cases to all 5 switches
   - Fix loop variable type (line 1044)
   - Remove tautological comparison (line 153)
   - Remove redundant cast (line 566)
   - Remove array address checks (lines 787, 792)
   - Remove extra parentheses (line 491)
   - Remove else-after-return (line 1667)
   - Remove else-after-break (line 1981)
   - Verify: Run array binding tests (007*.phpt)

3. **fbird_metadata.c** (15 warnings)
   - Remove all 7 always-true array address checks
   - Add 2 default cases to switches
   - Refactor 2 nested conditional operators
   - Remove else-after-continue
   - Remove else-after-return
   - Verify: Run field info tests (fbird_field_info*.phpt)

4. **fbird_blobs.c** (4 warnings)
   - Refactor `_php_fbird_string_to_quad()` - early return pattern
   - Refactor `_php_fbird_quad_to_string()` - early return + union cast
   - Add default case in `_php_fbird_blob_info()`
   - Verify: Run blob tests (004.phpt, fbird_blob*.phpt)

5. **fbird_events.c** (3 warnings)
   - Cast event_buf/result_buf to (ISC_SCHAR*) for isc_free()
   - Change loop variable from `unsigned short i` to `uint32_t i`
   - Verify: Run event tests (008*.phpt)

6. **fbird_result.c** (2 warnings)
   - Add default case to switch in `_php_fbird_fetch_hash()`
   - Fix loop variable type in `_php_fbird_fetch_array()`
   - Verify: Run result tests (003.phpt, returning*.phpt)

7. **fbird_service.c** (3 warnings)
   - Remove else-after-return
   - Add 2 default cases to switches
   - Verify: Run service tests (fbird_service*.phpt)

8. **Final Verification**
   - Run full `scripts/host/qa_full.sh`
   - Confirm 0 clang-tidy warnings
   - Confirm all 95 tests pass
   - Run on multiple PHP versions if possible

---

## Detailed Fix Patterns

### Pattern 1: else-after-return
```c
// BEFORE:
if (condition) {
    return x;
} else {
    // code
}

// AFTER:
if (condition) {
    return x;
}
// code
```

### Pattern 2: Missing default case
```c
// BEFORE:
switch (value) {
    case A: break;
    case B: break;
}

// AFTER:
switch (value) {
    case A: break;
    case B: break;
    default:
        break;  // or ZEND_UNREACHABLE() if truly unreachable
}
```

### Pattern 3: Array address always true
```c
// BEFORE:
const char *name = (var->sqlname && strlen(var->sqlname) > 0) ? var->sqlname : "";

// AFTER:
const char *name = (var->sqlname[0] != '\0') ? var->sqlname : "";
```

### Pattern 4: Loop variable too small
```c
// BEFORE:
unsigned short i;
for (i = 0; i < dim_len; ++i)  // dim_len is int

// AFTER:
int i;  // or uint32_t if needed
for (i = 0; i < dim_len; ++i)
```

### Pattern 5: Void pointer cast
```c
// BEFORE:
*(ISC_UINT64*)(void *) &qd

// AFTER:
// Use memcpy for type punning or union approach
union { ISC_QUAD q; ISC_UINT64 u; } conv;
conv.q = qd;
return conv.u;
```

### Pattern 6: Uninitialized variable
```c
// BEFORE:
int result;
// ... code path that may not assign result

// AFTER:
int result = FAILURE;  // Safe default
```

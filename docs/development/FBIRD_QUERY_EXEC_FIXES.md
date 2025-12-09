# fbird_query_exec.c - Critical Fixes Documentation

## Overview

This document tracks critical fixes in `fbird_query_exec.c` that improve 64-bit compatibility and input validation, along with their test coverage status.

---

## Fix 1: ISC_LONG Slice Length for 64-bit Systems

### Location
File: `fbird_query_exec.c`, function `_php_fbird_bind()`, lines ~406-409

### Problem
On 64-bit systems, the `ar->ar_size` field (type `zend_ulong`, 8 bytes on 64-bit) was being passed directly to `isc_array_put_slice()`, which expects an `ISC_LONG*` (4 bytes). This pointer type mismatch could cause:
- Incorrect slice length interpretation
- Potential memory corruption
- Undefined behavior on 64-bit platforms

### Solution
```c
/* FIX: Use temporary ISC_LONG for slice length to avoid pointer type mismatch on 64-bit systems */
ISC_LONG slice_len = (ISC_LONG)ar->ar_size;

if (isc_array_put_slice(IB_STATUS, &ib_query->link->handle.db, &ib_query->trans->handle.tr,
        &array_id, &ar->ar_desc, array_data, &slice_len)) {
    _php_ibase_error();
    efree(array_data);
    return FAILURE;
}
```

### Test Coverage Status
| Test File | Status | Reason |
|-----------|--------|--------|
| `tests/007.phpt` | **SKIPPED** | Known segfault in array handling (heap corruption during INSERT/FETCH) |

### Related Documentation
- See `docs/DEVELOPMENT_HISTORY.md` for project history

### Integration Notes
⚠️ **Caution**: While this fix addresses the 64-bit pointer type mismatch, the underlying array handling still has known issues that cause segfaults. The test remains skipped until the broader array handling issues are resolved.

**To verify this fix manually in the future when array handling is stable:**
1. Remove the `die("skip...")` line in `tests/007.phpt`
2. Run: `php run-tests.php tests/007.phpt`
3. Verify array operations complete without segfault and data integrity is maintained

---

## Fix 2: Enhanced Parameter Count Validation

### Location
File: `fbird_query_exec.c`, function `_php_fbird_exec()`, lines ~921-931

### Problem
Previous versions had weaker input validation that could lead to:
- Unclear error messages when parameter counts didn't match
- Potential crashes or unexpected behavior with invalid parameter counts
- Negative parameter counts not being properly handled

### Solution
```c
/* Enhanced parameter validation BEFORE Firebird API calls */
if (bind_n < 0 || argc < 0) {
    php_error_docref(NULL, E_WARNING, "Invalid parameter count: bind_n=%d, argc=%d", bind_n, argc);
    return FAILURE;
}

if (bind_n != argc) {
    php_error_docref(NULL, (bind_n < argc) ? E_WARNING : E_NOTICE,
        "Statement expects %d arguments, %d given", argc, bind_n);

    if (bind_n < argc) {
        return FAILURE;
    }
}
```

### Validation Behavior
| Condition | PHP Error Level | Behavior |
|-----------|-----------------|----------|
| `bind_n < argc` (too few args) | E_WARNING | Returns FAILURE (execution blocked) |
| `bind_n > argc` (too many args) | E_NOTICE | Continues execution (extra args ignored) |
| `bind_n < 0` or `argc < 0` | E_WARNING | Returns FAILURE (invalid state) |

### Test Coverage Status
| Test File | Status | Coverage |
|-----------|--------|----------|
| `tests/bug45373.phpt` | ✅ **ACTIVE** | Tests too many args (Notice) and too few args (Warning) |
| `tests/execute_safety_001.phpt` | ✅ **ACTIVE** | Tests API safety for new fbird_* functions |

### Test Expectations from `bug45373.phpt`
```php
// Too many arguments - continues with Notice
$r = ibase_execute($q, 1, 'test table not created with isql', 1);
// Notice: ibase_execute(): Statement expects 2 arguments, 3 given

// Too few arguments - blocked with Warning
$r = ibase_execute($q, 1);
// Warning: ibase_execute(): Statement expects 2 arguments, 1 given
```

### Documentation Impact
This stricter validation may cause existing code that was passing silently to now emit:
- **E_NOTICE** for extra parameters (previously silent)
- **E_WARNING** for insufficient parameters (may have been cryptic Firebird errors)

**User Action Required**: Developers should update code that passes incorrect parameter counts.

---

## Summary

| Fix | Description | Test Coverage | Recommendation |
|-----|-------------|---------------|----------------|
| ISC_LONG slice length | 64-bit pointer type safety for arrays | ⚠️ Test skipped (segfault issues) | Wait for array handling stabilization |
| Parameter validation | Clearer errors for parameter mismatches | ✅ Fully tested | Production ready |

---

## Version History
- **2025-12-01**: Initial documentation created
- **2025-12-09**: Renamed from IBASE_QUERY_EXEC_FIXES.md to FBIRD_QUERY_EXEC_FIXES.md

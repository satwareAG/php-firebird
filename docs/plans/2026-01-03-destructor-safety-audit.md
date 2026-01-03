# Destructor Safety Audit and Fix Plan

**Date:** 2026-01-03
**Related Issues:** [#56](https://github.com/satwareAG/php-firebird/issues/56), #22, #36, #50, #51, #55
**Status:** Audit Complete - Implementation Pending

## Executive Summary

Comprehensive audit of all resource destructors in php-firebird revealed that **5 of 8 destructors** are missing critical safety guards that were added to `fbird_service.c` to fix Issue #56. These missing guards can cause SIGSEGV crashes in:
- Forked child processes (PHPStan, PHPUnit parallel)
- Module shutdown scenarios
- Edge cases with NULL resource pointers

## The 5 Required Safety Guards

Based on the reference patterns in `_php_fbird_close_link()` and `_php_fbird_close_plink()`:

1. **NULL Pointer Check**: `if (rsrc->ptr == NULL) return;`
2. **Fork-Safety (Global)**: `if (IBG(init_pid) != 0 && current_pid != IBG(init_pid))` - skip Firebird API calls in forked children
3. **Fork-Safety (Resource)**: `if (resource->created_pid != 0 && current_pid != resource->created_pid)` - per-resource PID tracking
4. **MSHUTDOWN Guard**: `if (IBG(in_mshutdown))` - skip operations when module globals may be invalid
5. **OO API Validation**: Check `IBG(master_instance) != NULL` before any Firebird OO API calls

## Audit Results

### ✅ Properly Protected Destructors (No Changes Needed)

| Destructor | File | Guards |
|------------|------|--------|
| `_php_fbird_close_link()` | firebird.c | All 5 ✅ |
| `_php_fbird_close_plink()` | firebird.c | All 5 ✅ |
| `_php_fbird_free_service()` | fbird_service.c | All 5 ✅ (Fixed in Issue #56) |

### ❌ Destructors Requiring Fixes

#### 1. `_php_fbird_free_blob()` - fbird_blobs.c (Lines 177-199)

**Missing ALL 5 Guards!**

Current code:
```c
static void _php_fbird_free_blob(zend_resource *rsrc)
{
    fbird_blob *ib_blob = (fbird_blob *)rsrc->ptr;
    if (ib_blob->fbb_blob != NULL) { /* blob open */
        if (fbb_cancel(IBG(master_instance), ib_blob->fbb_blob, IB_STATUS) == 0) {
            // ... error handling
        }
        fbb_free(ib_blob->fbb_blob);
        ib_blob->fbb_blob = NULL;
    }
    efree(ib_blob);
}
```

**Required Changes:**
- Add NULL pointer check
- Add fork-safety checks (global + resource)
- Add MSHUTDOWN guard
- Add `IBG(master_instance)` validation
- Add `created_pid` field to `fbird_blob` struct

---

#### 2. `_php_fbird_free_event_rsrc()` - fbird_events.c (Lines 109-114)

**Missing ALL 5 Guards!**

Current code:
```c
static void _php_fbird_free_event_rsrc(zend_resource *rsrc)
{
    fbird_event *e = (fbird_event *) rsrc->ptr;
    _php_fbird_free_event(e);
    efree(e);
}
```

**Required Changes:**
- Add NULL pointer check
- Add fork-safety checks in `_php_fbird_free_event_rsrc()` AND `_php_fbird_free_event()`
- Add MSHUTDOWN guard
- Add `IBG(master_instance)` validation in `_php_fbird_free_event()` before line 66 (fbe_cancel call)
- Add `created_pid` field to `fbird_event` struct

---

#### 3. `php_fbird_free_query_rsrc()` - fbird_query_prepare.c (Lines 137-199)

**Missing 4 of 5 Guards** (Has NULL check only)

Current code already has:
```c
if (ib_query != NULL) {
    // ... cleanup
}
```

**Missing:**
- Fork-safety checks (global + resource)
- MSHUTDOWN guard
- `IBG(master_instance)` validation before OO API calls
- `created_pid` field tracking

**Required Changes:**
- Add fork-safety checks
- Add MSHUTDOWN guard
- Validate master_instance before `fbs_close_cursor()`, `fbs_free()` calls
- Add `created_pid` field to `fbird_query` struct

---

#### 4. `_php_fbird_free_trans()` - firebird.c (Lines 995-1032)

**Missing 4 of 5 Guards** (Has fork-safety global only)

Current code:
```c
fbird_transaction *trans = (fbird_transaction *)rsrc->ptr;
// ... NO NULL CHECK
#ifndef PHP_WIN32
if (IBG(init_pid) != 0 && getpid() != IBG(init_pid)) { // Has this
    // ...
}
#endif
if (trans->fbt_transaction != NULL) {
    fbt_rollback(trans->fbt_transaction, IB_STATUS);  // No master_instance check!
    // ...
}
```

**Required Changes:**
- Add NULL pointer check at start
- Add per-resource `created_pid` check
- Add MSHUTDOWN guard
- Add `IBG(master_instance)` validation before `fbt_rollback()`, `fbt_free()`
- Add `created_pid` field to `fbird_transaction` struct

---

#### 5. `_php_fbird_free_batch()` - firebird.c (Lines 1035-1071)

**Missing 4 of 5 Guards** (Has fork-safety global only)

Current code:
```c
fbird_batch *batch = (fbird_batch *)rsrc->ptr;
// ... NO NULL CHECK
#ifndef PHP_WIN32
if (IBG(init_pid) != 0 && getpid() != IBG(init_pid)) { // Has this
    // ...
}
#endif
if (batch->fbbatch_wrapper != NULL) {
    fbbatch_cancel(IBG(master_instance), ...);  // No master_instance check!
    // ...
}
```

**Required Changes:**
- Add NULL pointer check at start
- Add per-resource `created_pid` check
- Add MSHUTDOWN guard
- Add `IBG(master_instance)` validation before `fbbatch_cancel()`, `fbbatch_close()`, `fbm_release()`
- Add `created_pid` field to `fbird_batch` struct

## Struct Modifications Required

### 1. `fbird_blob` (php_firebird.h)
```c
typedef struct {
    isc_blob_handle bl_handle;
    int type;
    ISC_QUAD bl_qd;
    void *fbb_blob;
    pid_t created_pid;  // ADD THIS
} fbird_blob;
```

### 2. `fbird_event` (php_firebird.h)
```c
typedef struct _fbird_event {
    // ... existing fields ...
    pid_t created_pid;  // ADD THIS
} fbird_event;
```

### 3. `fbird_query` (php_fbird_query_internal.h or php_firebird.h)
```c
typedef struct {
    // ... existing fields ...
    pid_t created_pid;  // ADD THIS
} fbird_query;
```

### 4. `fbird_transaction` (php_firebird.h)
```c
typedef struct {
    // ... existing fields ...
    pid_t created_pid;  // ADD THIS
} fbird_transaction;
```

### 5. `fbird_batch` (php_firebird.h)
```c
typedef struct {
    // ... existing fields ...
    pid_t created_pid;  // ADD THIS
} fbird_batch;
```

## Implementation Order

1. **Phase 1: Header Changes** - Add `created_pid` to all 5 structs
2. **Phase 2: Creation Points** - Set `created_pid = getpid()` at resource allocation
3. **Phase 3: Destructor Guards** - Add all 5 safety guards to each destructor
4. **Phase 4: Testing** - Run PHPStan, PHPUnit parallel, fork tests

## Template for Destructor Fix

Based on `_php_fbird_free_service()` pattern:

```c
static void _php_fbird_free_RESOURCE(zend_resource *rsrc)
{
    fbird_RESOURCE *resource = (fbird_RESOURCE *)rsrc->ptr;

    /* Guard 1: NULL pointer check */
    if (resource == NULL) {
        return;
    }

#ifndef PHP_WIN32
    pid_t current_pid = getpid();

    /* Guard 2: Fork-safety (global) */
    if (IBG(init_pid) != 0 && current_pid != IBG(init_pid)) {
        FBDEBUG("Skipping RESOURCE cleanup in forked child (global)");
        efree(resource);
        return;
    }

    /* Guard 3: Fork-safety (resource) */
    if (resource->created_pid != 0 && current_pid != resource->created_pid) {
        FBDEBUG("Skipping RESOURCE cleanup in forked child (resource)");
        efree(resource);
        return;
    }
#endif

    /* Guard 4: MSHUTDOWN guard */
    if (IBG(in_mshutdown)) {
        FBDEBUG("Skipping RESOURCE cleanup during MSHUTDOWN");
        efree(resource);
        return;
    }

    /* Guard 5: OO API validation */
    if (IBG(master_instance) == NULL) {
        FBDEBUG("Skipping RESOURCE cleanup - master_instance is NULL");
        efree(resource);
        return;
    }

    /* ... actual cleanup using OO API ... */

    efree(resource);
}
```

## Files to Modify

| File | Changes |
|------|---------|
| `php_firebird.h` | Add `created_pid` to fbird_blob, fbird_event, fbird_transaction, fbird_batch structs |
| `php_fbird_query_internal.h` | Add `created_pid` to fbird_query struct |
| `fbird_blobs.c` | Fix `_php_fbird_free_blob()`, set created_pid in create functions |
| `fbird_events.c` | Fix `_php_fbird_free_event_rsrc()` and `_php_fbird_free_event()`, set created_pid |
| `fbird_query_prepare.c` | Fix `php_fbird_free_query_rsrc()`, set created_pid in `_php_fbird_prepare()` |
| `firebird.c` | Fix `_php_fbird_free_trans()`, `_php_fbird_free_batch()`, set created_pid in creation |

## Testing Strategy

1. **Unit Tests**: Existing PHPT tests should continue to pass
2. **Fork Tests**: Manual fork test script (`manual_fork_test.php`)
3. **PHPStan**: Run analysis with forked workers
4. **PHPUnit Parallel**: Run tests in parallel mode
5. **Valgrind**: Check for memory leaks in all paths

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Struct size changes breaking ABI | Only adds field at end, minor ABI change acceptable |
| Missed initialization points | Grep for all `emalloc(sizeof(fbird_*))` and `ecalloc` |
| Performance impact | Negligible - single getpid() call per destructor |
| Backward compatibility | None - internal implementation detail |

## References

- Issue #56: Service destructor segfault
- Issue #22: Fork safety for connections
- Issue #36: Per-resource PID tracking
- Issue #50, #51, #55: MSHUTDOWN safety
- `_php_fbird_close_link()` - Reference implementation with all guards

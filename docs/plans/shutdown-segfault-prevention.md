# Implementation Plan: Shutdown Segfault Prevention

[Overview]
Systematic prevention of SIGSEGV crashes during PHP shutdown by implementing comprehensive resource lifecycle guards, destructor hardening, and automated regression testing with Valgrind/ASAN.

The php-firebird extension has experienced recurring shutdown segfaults (Issues #50, #51, #55, #56, #64) caused by:
1. **Use-after-free**: Accessing freed memory in destructors during RSHUTDOWN/MSHUTDOWN
2. **EG() access during MSHUTDOWN**: Executor globals destroyed before persistent resource cleanup
3. **Resource ordering**: Parent resources freed before child resources
4. **Double-free**: SQLDA pointers and connection handles freed multiple times
5. **NULL dereference**: Accessing invalidated handles after explicit close/detach

This plan establishes a "defense-in-depth" approach with multiple safety layers and automated CI validation.

[Types]
Add centralized types and macros for consistent guard checking across all destructors.

```c
/* In php_fbird.h - Centralized guard macros */

/* Guard macro for all destructors - checks 4 safety conditions */
#define FBIRD_DESTRUCTOR_GUARD(resource_ptr, resource_name) \
    do { \
        /* Guard 1: NULL pointer check */ \
        if ((resource_ptr) == NULL) { \
            FBDEBUG(resource_name ": NULL pointer, skipping"); \
            return; \
        } \
        /* Guard 2: MSHUTDOWN flag check */ \
        if (IBG(in_mshutdown)) { \
            FBDEBUG(resource_name ": MSHUTDOWN active, skipping cleanup"); \
            return; \
        } \
    } while(0)

/* Guard macro for functions that return values */
#define FBIRD_FUNCTION_GUARD(handle_ptr, handle_name, return_val) \
    do { \
        if ((handle_ptr) == NULL) { \
            php_error_docref(NULL, E_WARNING, \
                "Supplied %s is not a valid handle", handle_name); \
            return (return_val); \
        } \
    } while(0)

/* Resource state flags */
typedef enum {
    FBIRD_STATE_ACTIVE = 0,
    FBIRD_STATE_DETACHED = 1,
    FBIRD_STATE_CLOSED = 2,
    FBIRD_STATE_ERROR = 3
} fbird_resource_state;

/* Extended resource tracking structure (optional for future) */
typedef struct {
    fbird_resource_state state;
    uint32_t ref_count;
    void *native_handle;
} fbird_resource_tracker;
```

[Files]
Modifications span destructor functions in 8 source files plus test additions.

**Files to Modify:**
1. `src/php_fbird.h` - Add centralized guard macros (FBIRD_DESTRUCTOR_GUARD, FBIRD_FUNCTION_GUARD)
2. `src/firebird.c` - Audit/update 6 destructors: `_php_fbird_close_link`, `_php_fbird_close_plink`, `php_fbird_commit_link_rsrc`, `_php_fbird_free_trans`, `_php_fbird_free_batch`, `_php_fbird_free_result`
3. `src/fbird_service.c` - Audit `_php_fbird_free_service`
4. `src/fbird_blobs.c` - Audit `_php_fbird_free_blob`
5. `src/fbird_events.c` - Audit `_php_fbird_free_event_rsrc`
6. `src/fbird_query_prepare.c` - Audit `php_fbird_free_query_rsrc`
7. `scripts/run-valgrind.sh` - Add shutdown-specific test mode
8. `scripts/run-sanitizer.sh` - Add shutdown-specific test mode

**Files to Create:**
1. `tests/shutdown_resource_cleanup.phpt` - Comprehensive shutdown regression test
2. `tests/shutdown_persistent_link.phpt` - Persistent connection shutdown test  
3. `tests/shutdown_nested_resources.phpt` - Parent/child resource ordering test
4. `tests/coverage/destructor_guards.phpt` - Destructor guard coverage test
5. `scripts/test-shutdown-safety.sh` - Automated shutdown safety validation script
6. `valgrind-shutdown.supp` - Suppressions specific to shutdown testing

[Functions]
Audit and standardize all destructor functions to use consistent guard patterns.

**Functions to Audit (Existing):**
1. `_php_fbird_close_link()` in `src/firebird.c` (~line 821) - Regular connection destructor
2. `_php_fbird_close_plink()` in `src/firebird.c` (~line 977) - Persistent connection destructor
3. `php_fbird_commit_link_rsrc()` in `src/firebird.c` (~line 862) - Request-end commit for persistent
4. `_php_fbird_free_trans()` in `src/firebird.c` (~line 1025) - Transaction destructor
5. `_php_fbird_free_batch()` in `src/firebird.c` (~line 1117) - Batch destructor
6. `_php_fbird_free_result()` in `src/fbird_result.c` - Result set destructor
7. `_php_fbird_free_service()` in `src/fbird_service.c` (~line 54) - Service manager destructor
8. `_php_fbird_free_blob()` in `src/fbird_blobs.c` (~line 189) - Blob destructor
9. `_php_fbird_free_event_rsrc()` in `src/fbird_events.c` (~line 127) - Event destructor
10. `php_fbird_free_query_rsrc()` in `src/fbird_query_prepare.c` (~line 151) - Query destructor

**Guard Pattern (Each Destructor):**
```c
static void _php_fbird_free_XXX(zend_resource *rsrc)
{
    resource_type *res = (resource_type *)rsrc->ptr;
    
    /* Guard 1: NULL pointer */
    if (res == NULL) {
        FBDEBUG("_php_fbird_free_XXX: NULL pointer");
        return;
    }
    
    /* Guard 2: Already cleaned up (state check) */
    if (res->native_handle == NULL) {
        FBDEBUG("_php_fbird_free_XXX: Already cleaned");
        efree(res);
        rsrc->ptr = NULL;
        return;
    }
    
    /* Guard 3: MSHUTDOWN phase - skip complex cleanup */
    if (IBG(in_mshutdown)) {
        FBDEBUG("_php_fbird_free_XXX: MSHUTDOWN, skipping");
        rsrc->ptr = NULL;
        return;
    }
    
    /* Guard 4: EG() access check (for destructors that modify lists) */
    /* - Use EG() only when NOT in persistent destructor context */
    
    /* Actual cleanup logic */
    // ... native handle cleanup ...
    
    /* Clear pointers BEFORE efree to prevent double-free */
    res->native_handle = NULL;
    efree(res);
    rsrc->ptr = NULL;
}
```

[Classes]
No class modifications required (extension is C-based, not OOP).

[Dependencies]
No new dependencies required. Existing tools:
- Valgrind (already in docker containers)
- AddressSanitizer (php83-asan container exists)
- UndefinedBehaviorSanitizer (optional)

[Testing]
Create comprehensive shutdown regression test suite with Valgrind/ASAN validation.

**Test Categories:**

1. **Resource Lifecycle Tests** (`tests/shutdown_*.phpt`)
   - Normal shutdown with open resources
   - Shutdown with explicit close before exit
   - Shutdown with detached service handles
   - Shutdown with nested transactions
   - Shutdown with open blobs
   - Shutdown with active queries
   - Shutdown with persistent connections

2. **Destructor Guard Coverage** (`tests/coverage/destructor_guards.phpt`)
   - NULL pointer handling in each destructor
   - Double-close handling
   - Use-after-detach handling
   - MSHUTDOWN flag validation

3. **Stress/Edge Cases** (`tests/shutdown_stress.phpt`)
   - Multiple connections abandoned
   - Deep transaction nesting
   - Blob streaming interrupted by exit
   - Event handlers during shutdown

**Validation Script** (`scripts/test-shutdown-safety.sh`):
```bash
#!/bin/bash
# Run shutdown tests with memory checking tools
# Usage: ./scripts/test-shutdown-safety.sh [--valgrind|--asan|--all]

# 1. Run tests normally first
# 2. Run with Valgrind (detect use-after-free, uninitialized reads)
# 3. Run with ASAN (detect memory corruption)
# 4. Report any segfaults or memory errors
```

**CI Integration:**
- Add `test-shutdown-safety` job to `.github/workflows/ci.yml`
- Run on every PR that modifies destructor code
- Block merge if any shutdown tests fail with Valgrind/ASAN

[Implementation Order]
Phased implementation to minimize risk and ensure each layer is validated.

1. **Phase 1: Add Centralized Guard Macros** (30 min)
   - Add `FBIRD_DESTRUCTOR_GUARD` macro to `php_fbird.h`
   - Add `FBIRD_FUNCTION_GUARD` macro to `php_fbird.h`
   - Compile and verify no regressions

2. **Phase 2: Audit Existing Destructors** (1 hour)
   - Review each destructor for guard completeness
   - Document any missing guards
   - Create checklist of required changes

3. **Phase 3: Standardize Destructor Guards** (2 hours)
   - Apply consistent guard pattern to all 10 destructors
   - Ensure rsrc->ptr = NULL after cleanup
   - Add FBDEBUG logging for each guard path
   - Compile after each file change

4. **Phase 4: Create Shutdown Regression Tests** (1 hour)
   - Create `tests/shutdown_resource_cleanup.phpt`
   - Create `tests/shutdown_persistent_link.phpt`
   - Create `tests/shutdown_nested_resources.phpt`
   - Verify tests pass without Valgrind first

5. **Phase 5: Create Validation Script** (30 min)
   - Create `scripts/test-shutdown-safety.sh`
   - Integrate with existing `run-valgrind.sh` and `run-sanitizer.sh`
   - Test script execution

6. **Phase 6: Valgrind/ASAN Validation** (1 hour)
   - Run all shutdown tests with Valgrind
   - Run all shutdown tests with ASAN
   - Fix any detected issues
   - Create suppressions for known-safe warnings

7. **Phase 7: CI Integration** (30 min)
   - Add shutdown safety job to CI workflow
   - Configure as blocking for merge
   - Document in CONTRIBUTING.md

8. **Phase 8: Documentation** (30 min)
   - Update docs/DEVELOPMENT_HISTORY.md
   - Add shutdown safety section to README
   - Create docs/development/shutdown-safety.md

**Total Estimated Time: ~7 hours**

---

## Appendix: Existing Guard Inventory

| File | Destructor | Guard 1 (NULL) | Guard 2 (State) | Guard 3 (MSHUTDOWN) | Guard 4 (EG) |
|------|-----------|----------------|-----------------|---------------------|--------------|
| firebird.c | _php_fbird_close_link | ❓ | ❓ | ✅ | ✅ |
| firebird.c | _php_fbird_close_plink | ❓ | ❓ | ✅ | ✅ |
| firebird.c | _php_fbird_free_trans | ❓ | ❓ | ✅ | ❓ |
| firebird.c | _php_fbird_free_batch | ❓ | ❓ | ✅ | ❓ |
| fbird_service.c | _php_fbird_free_service | ❓ | ❓ | ✅ | ❓ |
| fbird_blobs.c | _php_fbird_free_blob | ❓ | ❓ | ✅ | ❓ |
| fbird_events.c | _php_fbird_free_event_rsrc | ❓ | ❓ | ✅ | ❓ |
| fbird_query_prepare.c | php_fbird_free_query_rsrc | ❓ | ❓ | ✅ | ❓ |

Legend: ✅ = Present, ❓ = Needs Audit, ❌ = Missing

## Appendix: Related Issues

- Issue #50: SIGSEGV exit code 139 during shutdown
- Issue #51: EG() access during MSHUTDOWN
- Issue #55: NULL pointer guards to resource destructors
- Issue #56: Use-after-free in fbird_drop_db with persistent connections
- Issue #64: Service API functions crash after detach

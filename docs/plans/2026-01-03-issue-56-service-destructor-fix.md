# Implementation Plan: Issue #56 - Service Destructor Safety Guards

[Overview]
Fix segfault in `fb::ServiceWrapper::detach()` during PHP request shutdown by adding comprehensive safety guards to `_php_fbird_free_service()`.

This implementation addresses GitHub Issue #56 where the service manager destructor crashes with SIGSEGV at `si_addr=0x4` (NULL + offset pattern) when `m_master->getStatus()` is called on an invalid master instance. The crash occurs during `php_request_shutdown()` when resources are being cleaned up.

**Root Cause Analysis:**
- The `_php_fbird_free_service()` destructor in `fbird_service.c` is missing ALL safety guards that other destructors in the extension have
- Working destructors like `_php_fbird_close_link()` and `_php_fbird_free_trans()` already implement proper patterns
- The crash signature `si_addr=0x4` indicates NULL pointer dereference with struct field offset

**Best Practices Applied (Research Sources):**
1. **PHP 8.1 Core (mysqli, PDO)**: `if (res->ptr)` guards, multi-pointer validation before method calls
2. **phpredis (10k stars)**: Resource destructors with pointer checks, explicit disconnection
3. **PHP Internals Book**: RSHUTDOWN called after object destructors, resource cleanup order
4. **php-firebird existing patterns**: Two-level fork detection, MSHUTDOWN guards

[Types]
The `fbird_service` struct needs a new field for per-service fork tracking.

**Modified struct in `fbird_service.c`:**
```c
typedef struct {
    void *handle;           /* void* to support 64-bit handles */
    char *hostname;
    char *username;
    zend_resource *res;
    void *fbsvc_service;    /* OO API ServiceWrapper* (Phase 8) */
    pid_t created_pid;      /* NEW: PID when service was created (fork detection) */
} fbird_service;
```

This follows the same pattern as `fbird_db_link` which has `created_pid` for connection-level fork detection.

[Files]
Modifications to fix Issue #56 and align service destructor with best practices.

**Files to modify:**
1. `fbird_service.c` - Main changes:
   - Add `created_pid` field to `fbird_service` struct
   - Rewrite `_php_fbird_free_service()` with comprehensive safety guards
   - Initialize `created_pid` in `PHP_FUNCTION(fbird_service_attach)`

2. `CHANGELOG.md` - Document the fix under v7.0.0-rc section

**No new files required.**
**No files to delete.**

[Functions]
Function modifications for the safety fix.

**Modified Functions:**

1. **`_php_fbird_free_service(zend_resource *rsrc)`** in `fbird_service.c`
   - Current: No safety guards, directly accesses pointers
   - Required changes:
     - Add NULL pointer guard for `rsrc->ptr`
     - Add fork-safety check using `IBG(init_pid)` (global level)
     - Add fork-safety check using `sv->created_pid` (service level)
     - Add `master_instance` validation before OO API calls
     - Add MSHUTDOWN guard for operations that may access engine state
     - Ensure proper cleanup order (OO API first, then legacy API)

2. **`PHP_FUNCTION(fbird_service_attach)`** in `fbird_service.c`
   - Current: Does not set `created_pid`
   - Required changes:
     - Initialize `svm->created_pid = getpid()` after allocation

**New helper (optional):**
- Consider extracting guard logic to a macro if reused, but inline is acceptable for clarity

[Classes]
No class modifications required - the C++ `ServiceWrapper` class is already well-implemented with proper NULL checks in its methods.

The `ServiceWrapper::detach()` method already checks:
- `if (!m_service) return true;` - Already detached
- `if (!m_master) return false;` - Invalid state

The issue is at the C layer calling into C++ with invalid global state.

[Dependencies]
No new dependencies required.

Existing dependencies used:
- `<unistd.h>` for `getpid()` (already included via PHP headers)
- `IBG()` macro for module globals access (already available)

[Testing]
Testing approach for Issue #56 fix.

**Test Strategy:**
1. **Existing PHPT tests** - Run full test suite to ensure no regressions
2. **Manual fork test** - Use `manual_fork_test.php` to verify fork safety
3. **Service-specific tests** - Check `tests/fbird_service_*.phpt` if they exist

**Validation Commands:**
```bash
# Run QA script
./scripts/qa.sh

# Run service-related tests specifically
docker compose exec php83-dev php run-tests.php -p php tests/fbird_*service*.phpt

# Run with AddressSanitizer if available
./scripts/run-sanitizer.sh
```

**Expected behavior after fix:**
- No SIGSEGV during request shutdown with service handles
- Service cleanup silently skipped in forked child processes
- Service cleanup silently skipped during MSHUTDOWN when globals invalid
- Graceful handling of NULL or invalid resource pointers

[Implementation Order]
Sequential implementation steps to minimize risk.

1. **Modify `fbird_service` struct** - Add `created_pid` field
   - Risk: Low (additive change)
   - Validation: Compile succeeds

2. **Update `PHP_FUNCTION(fbird_service_attach)`** - Initialize `created_pid`
   - Risk: Low (initialization only)
   - Validation: Service attach still works

3. **Rewrite `_php_fbird_free_service()`** - Add all safety guards
   - Risk: Medium (core fix)
   - Validation: Manual testing, PHPT tests pass

4. **Run full QA suite** - Ensure no regressions
   - Validation: `./scripts/qa.sh` passes

5. **Update CHANGELOG.md** - Document fix
   - Version: 7.0.0-rc.39

6. **Commit and tag** - Incremental commit <200 LOC

**Implementation Details for Step 3 (Core Fix):**

```c
static void _php_fbird_free_service(zend_resource *rsrc)
{
    fbird_service *sv = (fbird_service *) rsrc->ptr;

    /* Guard 1: NULL pointer check (Issue #55 pattern)
     * In forked PHP workers, rsrc->ptr may be NULL when inherited resource
     * descriptors are destroyed during child process shutdown. */
    if (sv == NULL) {
        return;
    }

#ifndef PHP_WIN32
    /* Guard 2: Fork-safety - Global level (Issue #22, #36 pattern)
     * After pcntl_fork(), child inherits global state including master_instance.
     * Attempting to detach handles in child that were created in parent causes
     * segfault. Only the original process should perform cleanup. */
    pid_t current_pid = getpid();
    if (IBG(init_pid) != 0 && current_pid != IBG(init_pid)) {
        /* In forked child - just free the struct, don't call Firebird API */
        if (sv->hostname) {
            efree(sv->hostname);
        }
        if (sv->username) {
            efree(sv->username);
        }
        efree(sv);
        return;
    }

    /* Guard 3: Fork-safety - Service level
     * Even if global init_pid matches, this specific service may have been
     * created in a different process (e.g., worker spawned after module init). */
    if (sv->created_pid != 0 && current_pid != sv->created_pid) {
        if (sv->hostname) {
            efree(sv->hostname);
        }
        if (sv->username) {
            efree(sv->username);
        }
        efree(sv);
        return;
    }
#endif

    /* Guard 4: MSHUTDOWN safety
     * During module shutdown, master_instance may be in undefined state.
     * Skip API calls but still free PHP-allocated memory. */
    if (IBG(in_mshutdown)) {
        if (sv->hostname) {
            efree(sv->hostname);
        }
        if (sv->username) {
            efree(sv->username);
        }
        efree(sv);
        return;
    }

    /* Guard 5: master_instance validation before OO API calls */
    if (sv->fbsvc_service != NULL && IBG(master_instance) != NULL) {
        fbsvc_detach(IBG(master_instance), sv->fbsvc_service, IB_STATUS);
        fbsvc_free(sv->fbsvc_service);
        sv->fbsvc_service = NULL;
    }

    /* Legacy API cleanup - only if handle is valid */
    if (sv->handle != 0) {
        if (isc_service_detach(IB_STATUS, (isc_svc_handle *)&sv->handle)) {
            _php_fbird_error();
        }
    }

    /* Free PHP-allocated memory */
    if (sv->hostname) {
        efree(sv->hostname);
    }
    if (sv->username) {
        efree(sv->username);
    }

    efree(sv);
}

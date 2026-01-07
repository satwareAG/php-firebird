# Implementation Plan: Fork Safety with pthread_atfork()

## [Overview]
Implement proper fork safety for the PHP Firebird extension using the industry-standard `pthread_atfork()` pattern combined with per-request PID tracking.

The current implementation attempts fork detection using per-connection `created_pid` fields, which fails when connections are reused from cache (leaving `created_pid=0` or stale). When `pcntl_fork()` is called, the child process inherits all PHP resources including database connections. During child shutdown, destructors run and attempt to close Firebird handles, corrupting the parent's active socket connection.

Research shows the proper 2025/2026 solution is:
1. **pthread_atfork()**: Register fork handlers to mark resources as "forked"
2. **Per-request PID**: Track in RINIT/RSHUTDOWN instead of per-connection
3. **Child handler**: Set flag to prevent all Firebird API calls in child
4. **Destructor checks**: Skip cleanup when fork flag is set

This approach is used by PostgreSQL, MySQL, gRPC, and other database extensions.

## [Types]
Add fork-safety tracking to module globals.

```c
/* In php_fbird_includes.h, add to ZEND_BEGIN_MODULE_GLOBALS(fbird): */
ZEND_BEGIN_MODULE_GLOBALS(fbird)
	/* ... existing fields ... */
	pid_t init_pid;                 /* PID at GINIT (existing) */
	pid_t request_pid;              /* PID at request start (NEW) */
	int in_forked_child;            /* Flag: 1 if child of pcntl_fork() (NEW) */
	zend_bool in_mshutdown;         /* Flag: 1 during MSHUTDOWN (existing) */
ZEND_END_MODULE_GLOBALS(fbird)
```

No changes to resource structs needed - the per-connection `created_pid` fields can be removed in future cleanup.

## [Files]
Modify existing files to implement fork-safety handlers and request-level PID tracking.

**Modified Files**:
- `src/firebird.c`:
  - Add `PHP_RINIT_FUNCTION(fbird)` to track request PID
  - Add pthread_atfork() registration in `PHP_MINIT_FUNCTION`
  - Add fork handler functions (prepare, parent, child)
  - Update all destructors to check `IBG(in_forked_child)` flag
  - Update module entry to register RINIT

- `include/php_fbird_includes.h`:
  - Add `request_pid` field to module globals
  - Add `in_forked_child` flag to module globals

**No New Files**: All changes are modifications to existing code.

**No Deleted Files**: This is an enhancement, not a refactor.

## [Functions]
Add RINIT function and pthread_atfork() handlers, modify existing destructors.

**New Functions**:

1. `PHP_RINIT_FUNCTION(fbird)` in `src/firebird.c`:
   ```c
   PHP_RINIT_FUNCTION(fbird)
   {
       #ifndef PHP_WIN32
       IBG(request_pid) = getpid();
       IBG(in_forked_child) = 0;
       #endif
       return SUCCESS;
   }
   ```
   Purpose: Track current request's PID, reset fork flag

2. `static void fbird_atfork_prepare(void)` in `src/firebird.c`:
   ```c
   static void fbird_atfork_prepare(void)
   {
       /* Called before fork() - nothing needed for Firebird */
   }
   ```
   Purpose: Pre-fork preparation (no locks needed for Firebird)

3. `static void fbird_atfork_parent(void)` in `src/firebird.c`:
   ```c
   static void fbird_atfork_parent(void)
   {
       /* Parent continues normally - no action needed */
   }
   ```
   Purpose: Post-fork parent handler

4. `static void fbird_atfork_child(void)` in `src/firebird.c`:
   ```c
   static void fbird_atfork_child(void)
   {
       #ifndef PHP_WIN32
       IBG(in_forked_child) = 1;
       IBG(request_pid) = getpid();
       #endif
   }
   ```
   Purpose: Mark child process to skip all Firebird API cleanup

**Modified Functions**:

1. `PHP_MINIT_FUNCTION(fbird)` in `src/firebird.c`:
   - Add `pthread_atfork()` registration after REGISTER_INI_ENTRIES()
   ```c
   #ifndef PHP_WIN32
   pthread_atfork(fbird_atfork_prepare, fbird_atfork_parent, fbird_atfork_child);
   #endif
   ```

2. `PHP_GINIT_FUNCTION(fbird)` in `src/firebird.c`:
   - Initialize new fields:
   ```c
   #ifndef PHP_WIN32
   fbird_globals->request_pid = 0;
   fbird_globals->in_forked_child = 0;
   #endif
   ```

3. `_php_fbird_commit_link()` in `src/firebird.c`:
   - Add fork check at start:
   ```c
   #ifndef PHP_WIN32
   if (IBG(in_forked_child)) {
       FBDEBUG("_php_fbird_commit_link: Skipping in forked child");
       return;
   }
   #endif
   ```

4. `php_fbird_commit_link_rsrc()` in `src/firebird.c`:
   - Add NULL check and fork check before calling `_php_fbird_commit_link()`

5. `_php_fbird_close_link()` in `src/firebird.c`:
   - Replace per-connection PID checks with fork flag check

6. `_php_fbird_close_plink()` in `src/firebird.c`:
   - Replace per-connection PID checks with fork flag check

7. `_php_fbird_free_trans()` in `src/firebird.c`:
   - Replace per-connection PID checks with fork flag check

8. `_php_fbird_free_batch()` in `src/firebird.c` (if FB_API_VER >= 40):
   - Replace per-connection PID checks with fork flag check

9. Module entry in `src/firebird.c`:
   - Add RINIT registration:
   ```c
   PHP_RINIT(fbird),  /* Replace NULL */
   ```

**Removed per-connection PID Checks**:
- All `link->created_pid != current_pid` checks replaced with `IBG(in_forked_child)` check
- Simpler, more reliable, matches industry patterns

## [Classes]
No classes in C extension - not applicable.

## [Dependencies]
Add pthread dependency (already present on all target platforms).

**Build System**:
- No new dependencies - pthread is standard POSIX
- Conditional compilation with `#ifndef PHP_WIN32` (Windows doesn't support fork)
- Requires `<pthread.h>` include (add to firebird.c)

**Platform Compatibility**:
- Linux: ✅ pthread_atfork() fully supported
- macOS: ✅ pthread_atfork() fully supported
- FreeBSD: ✅ pthread_atfork() fully supported
- Windows: ⚠️ No fork support, gracefully skipped with `#ifndef PHP_WIN32`

## [Testing]
Update fork safety test to verify proper behavior, add new test cases.

**Test Modifications**:
1. `tests/uaf_fork_safety.phpt`:
   - Remove XFAIL marker
   - Test should now pass with pthread_atfork() implementation

**New Test Cases** (optional enhancements):
1. `tests/fork_safety_multiple_forks.phpt`:
   - Test multiple sequential forks
   - Verify parent resources remain valid after each child exits

2. `tests/fork_safety_nested.phpt`:
   - Test nested forks (child forks grandchild)
   - Verify flag propagates correctly

**Validation Strategy**:
- All 168 tests must pass
- Specifically verify fork safety test passes
- Run with Valgrind to ensure no memory leaks
- Test with PHPStan parallel mode (real-world fork scenario)

## [Implementation Order]
Implement changes in logical dependency order to maintain working state.

### Phase 1: Add Infrastructure (Non-Breaking)
1. Add `request_pid` and `in_forked_child` fields to module globals in `php_fbird_includes.h`
2. Initialize new fields in `PHP_GINIT_FUNCTION(fbird)`
3. Add `PHP_RINIT_FUNCTION(fbird)` to track request PID
4. Update module entry to register RINIT function
5. Compile and verify extension still loads (no functional changes yet)

### Phase 2: Add Fork Handlers
6. Add `#include <pthread.h>` to `src/firebird.c`
7. Implement `fbird_atfork_prepare()` (empty stub)
8. Implement `fbird_atfork_parent()` (empty stub)
9. Implement `fbird_atfork_child()` (set `in_forked_child` flag)
10. Register fork handlers in `PHP_MINIT_FUNCTION` with `pthread_atfork()`
11. Compile and test - fork flag should be set in child processes

### Phase 3: Update Destructors (Core Fix)
12. Update `_php_fbird_commit_link()`: Add `in_forked_child` check at start
13. Update `php_fbird_commit_link_rsrc()`: Add `in_forked_child` check
14. Update `_php_fbird_close_link()`: Replace PID checks with fork flag check
15. Update `_php_fbird_close_plink()`: Replace PID checks with fork flag check
16. Update `_php_fbird_free_trans()`: Replace PID checks with fork flag check
17. Update `_php_fbird_free_batch()`: Replace PID checks with fork flag check (FB 4.0+)
18. Compile and run all tests

### Phase 4: Cleanup and Documentation
19. Remove XFAIL from `tests/uaf_fork_safety.phpt`
20. Verify test passes
21. Run full test suite - expect 168/168 passing
22. Update CHANGELOG.md with fork safety fix
23. Document pthread_atfork() usage in code comments

### Dependencies Between Steps
- Steps 1-5 must complete before Step 6 (infrastructure first)
- Steps 6-11 must complete before Step 12 (handlers before use)
- Steps 12-18 can be done in parallel or sequentially
- Step 19-21 validate all previous work
- Step 22-23 are documentation only

### Rollback Strategy
If pthread_atfork() causes issues:
1. Remove pthread_atfork() registration from MINIT
2. Keep RINIT PID tracking
3. Fall back to request_pid comparison in destructors
4. Still an improvement over current per-connection tracking

### Success Criteria
- ✅ Fork safety test passes without XFAIL
- ✅ All 168 tests pass
- ✅ No Valgrind errors
- ✅ PHPStan parallel mode works without crashes
- ✅ No performance regression (pthread_atfork() is lightweight)

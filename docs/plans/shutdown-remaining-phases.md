# Implementation Plan: Shutdown Safety Remaining Phases

[Overview]
Complete the remaining phases of shutdown segfault prevention for php-firebird.

Phases 1-2 (macros and audit) are complete. This plan covers Phases 2b (remaining audits), 3-8 (tests, validation, CI integration, documentation). The goal is to create comprehensive shutdown tests, validation scripts, and ensure CI catches regressions.

[Types]
No new types required for remaining phases.

The `fbird_resource_state` enum and guard macros were added in Phase 1.

[Files]
Files to complete audit and create new test infrastructure.

**Audit (Phase 2b - remaining destructors):**
- `src/fbird_blobs.c` - Audit `_php_fbird_free_blob()`
- `src/fbird_events.c` - Audit `_php_fbird_free_event_rsrc()`
- `src/fbird_query_prepare.c` - Audit `php_fbird_free_query_rsrc()`

**New test files (Phase 4):**
- `tests/shutdown_resource_cleanup.phpt` - Basic shutdown test
- `tests/shutdown_persistent_link.phpt` - pconnect shutdown test
- `tests/shutdown_nested_resources.phpt` - Transaction/query/blob cleanup

**New scripts (Phase 5):**
- `scripts/test-shutdown-safety.sh` - Valgrind/ASAN validation script

**CI modifications (Phase 7):**
- `.github/workflows/ci.yml` - Add shutdown-safety job

**Documentation (Phase 8):**
- `docs/SECURITY.md` - Add shutdown safety section
- `CHANGELOG.md` - Document improvements

[Functions]
Functions to audit for guard completeness.

**Remaining destructor audits:**
- `_php_fbird_free_blob()` in `src/fbird_blobs.c` - NULL, MSHUTDOWN, fork-safety
- `_php_fbird_free_event_rsrc()` in `src/fbird_events.c` - NULL, MSHUTDOWN, fork-safety
- `php_fbird_free_query_rsrc()` in `src/fbird_query_prepare.c` - NULL, MSHUTDOWN, fork-safety

[Classes]
No class modifications required.

This extension is primarily C code without OOP classes.

[Dependencies]
No new dependencies required.

Valgrind and AddressSanitizer are used for validation but are existing tools.

[Testing]
Create three specialized shutdown test files.

**Test 1: `tests/shutdown_resource_cleanup.phpt`**
```
--TEST--
Shutdown resource cleanup - basic safety
--FILE--
<?php
// Create resources, let PHP garbage collect at shutdown
$db = fbird_connect($host, $user, $pass);
$trans = fbird_trans($db);
$query = fbird_prepare($db, "SELECT 1 FROM RDB$DATABASE");
// No explicit cleanup - tests destructor safety
?>
--EXPECT--
(no output - clean shutdown)
```

**Test 2: `tests/shutdown_persistent_link.phpt`**
```
--TEST--
Shutdown persistent connection cleanup safety
--FILE--
<?php
$db = fbird_pconnect($host, $user, $pass);
$trans = fbird_trans($db);
// pconnect resources persist - tests MSHUTDOWN paths
?>
--EXPECT--
(no output - clean shutdown)
```

**Test 3: `tests/shutdown_nested_resources.phpt`**
```
--TEST--
Shutdown nested resource cleanup order
--FILE--
<?php
$db = fbird_connect($host, $user, $pass);
$trans = fbird_trans($db);
$stmt = fbird_prepare($db, "SELECT * FROM test_table");
fbird_execute($stmt);
$blob = fbird_blob_create($db);
// Deep nesting - tests destructor ordering
?>
--EXPECT--
(no output - clean shutdown)
```

**Validation script: `scripts/test-shutdown-safety.sh`**
Runs shutdown tests under Valgrind and ASAN, checking for:
- No SIGSEGV crashes
- No memory leaks in destructors
- No invalid memory access
- Clean exit codes

[Implementation Order]
Execute phases in order to ensure safe incremental progress.

1. **Phase 2b: Complete remaining destructor audits**
   - Audit `_php_fbird_free_blob()` for NULL/MSHUTDOWN/fork guards
   - Audit `_php_fbird_free_event_rsrc()` for guard completeness
   - Audit `php_fbird_free_query_rsrc()` for guard completeness
   - Document findings or apply FBIRD_DESTRUCTOR_GUARD if missing

2. **Phase 3: Apply guards to any lacking destructors** (if needed)
   - Only if Phase 2b finds gaps

3. **Phase 4: Create shutdown test files**
   - Create `tests/shutdown_resource_cleanup.phpt`
   - Create `tests/shutdown_persistent_link.phpt`
   - Create `tests/shutdown_nested_resources.phpt`
   - Run tests to verify they pass

4. **Phase 5: Create validation script**
   - Create `scripts/test-shutdown-safety.sh`
   - Make script executable
   - Test locally

5. **Phase 6: Run memory validation**
   - Run shutdown tests with Valgrind
   - Run shutdown tests with ASAN
   - Fix any detected memory issues

6. **Phase 7: Add CI job**
   - Add shutdown-safety job to `.github/workflows/ci.yml`
   - Ensure job runs on PR and push

7. **Phase 8: Update documentation**
   - Add shutdown safety section to `docs/SECURITY.md`
   - Update `CHANGELOG.md` with improvements

8. **Final: Git commit and push**
   - Commit all changes with appropriate message
   - Push to remote

# Implementation Plan - Segfault Debugging Toolsuite

[Overview]
Establish a comprehensive debugging environment for hard-to-reproduce segfaults in `php-firebird` running in Docker, specifically targeting crashes during PHP shutdown (exit code 139). The crash pattern suggests resource cleanup issues in forked processes (PHPStan parallel mode).

[Types]
No new PHP/C types files are needed.

[Files]
New files to be created:
- `scripts/debug_segfault.sh`: Main entry point for debugging. Orchestrates tools (Valgrind, GDB, scan-build).
- `tests/stress_shutdown.phpt`: Official PHPT stress test targeting shutdown crashes. Exercises connection, transaction, introspection, and explicit cleanup paths.

[Functions]
New functions in `scripts/debug_segfault.sh`:
- `run_valgrind()`: Executes `tests/stress_shutdown.phpt` via Valgrind with `USE_ZEND_ALLOC=0` and `ZEND_DONT_UNLOAD_MODULES=1`.
- `run_gdb()`: Executes under GDB in batch mode, trapping SIGSEGV to capture `bt full`.
- `run_strace()`: Traces system calls to identify signal origins.

[Dependencies]
Tools already in `php81-fb3-dev` container: `valgrind`, `gdb`, `strace`, `clang-tools` (scan-build).

[Shutdown Debugging Strategy]
Based on `firebird.c` analysis:
1. **Critical Guards**: The extension has `IBG(in_mshutdown)` flag and `created_pid` fork-safety checks in ALL destructors.
2. **Known Issue**: Issue #55 describes `si_addr=0x4` (NULL + offset) crash pattern in PHPStan workers.
3. **Root Cause Hypothesis**: Gap in fork-safety guard coverage OR race condition in persistent link hash cleanup.

[Stress Test Design - tests/stress_shutdown.phpt]
The test MUST exercise EVERY destructor path mentioned in `firebird.c`:
1. **Persistent Connections**: `fbird_pconnect()` creates `le_plink` resources.
2. **Normal Connections**: `fbird_connect()` creates `le_link` resources.
3. **Transactions**: Explicit `fbird_trans()` creates `le_trans` resources + unterminated (implicit).
4. **Batches** (FB >= 4.0): `fbird_batch_create()` creates `le_batch` resources if API available.
5. **Reflection**: `ReflectionExtension('firebird')` loads function/class metadata.
6. **Multiple Iterations**: Loop to stress memory allocator.
7. **Exit Without Explicit Cleanup**: Let destructors run at shutdown.

[Manual Code Review Checklist]
If automated tools fail to catch the issue, perform manual review on:
- **ZVAL Handling**: Verify non-NULL pointers after `zend_parse_parameters`.
- **Handle Initialization**: Ensure `isc_db_handle`/`isc_tr_handle` are 0/NULL before use.
- **Resource Destructors**: Check `MINIT` registration and `efree` usage against PHP 8 GC.
- **Fork Guards**: Verify ALL exit paths in `_php_fbird_close_link`, `_php_fbird_close_plink`, `_php_fbird_free_trans`, `_php_fbird_free_batch` have consistent `created_pid` checks.

[Implementation Order]
1. Create `tests/stress_shutdown.phpt` (self-contained PHPT, exercises all resource types).
2. Create `scripts/debug_segfault.sh` wrapper with GDB/Valgrind/strace integration.
3. Run stress test under Valgrind in `php81-fb3-dev` container.
4. Analyze output, correlate with `firebird.c` destructor code.

task_progress Items:
- [x] Create `tests/stress_shutdown.phpt` (Persistent/Normal connections, Transactions, Batches, Reflection)
- [x] Create `scripts/debug_segfault.sh` wrapper with shutdown debugging strategy
- [x] Verify execution in `php81-fb3-dev` container (2026-01-04)
- [x] Document findings in `docs/research/segfault-analysis.md` (2026-01-04)

## Findings Summary (2026-01-04)

**12 Memory Errors Detected by Valgrind:**

The stress test successfully identified a **use-after-free bug** in persistent connection cleanup:

1. `fbird_drop_db()` deletes the Connection object during RSHUTDOWN
2. `_php_fbird_close_plink()` later tries to use the already-freed object during MSHUTDOWN
3. Results in invalid reads and double-free errors

**Affected Files:**
- `firebird.c:987, 997` - `_php_fbird_close_plink()` 
- `firebird_utils.cpp:537-544` - `fbc_disconnect()`
- `firebird_utils.cpp:573` - `fbc_drop_database()`

**Recommended Fix:**
After `fbc_drop_database()` deletes the Connection, remove the persistent link entry from the hash table or NULL out the Connection pointer and add a check in `_php_fbird_close_plink()`.

# Implementation Plan: Destructor Safety Guards

[Overview]
Add fork-safety and MSHUTDOWN guards to 5 resource destructors to prevent SIGSEGV crashes in forked child processes and during module shutdown.

This implementation addresses the same vulnerability pattern fixed in Issue #56 (`_php_fbird_free_service`). After comprehensive audit of all 8 resource destructors, we found that 5 are missing critical safety guards that protect against:
- Forked child processes attempting to use Firebird handles created by parent
- Module shutdown scenarios where executor globals are already destroyed
- NULL pointer dereferences when inherited resource descriptors have NULL ptr

The fix follows the established pattern in `_php_fbird_close_link()` and `_php_fbird_close_plink()` which have all 5 guards. The fix for `_php_fbird_free_service()` serves as the template.

[Types]
Add `created_pid` field to 5 struct definitions in php_fbird_includes.h.

**Struct: fbird_blob (lines 140-148)**
Add after `void *fbb_blob;`:
```c
#ifndef PHP_WIN32
    pid_t created_pid;   /* PID when blob was created (fork detection) */
#endif
```

**Struct: fbird_event (lines 150-171)**
Add after `void *fbe_events;`:
```c
#ifndef PHP_WIN32
    pid_t created_pid;   /* PID when event was created (fork detection) */
#endif
```

**Struct: fbird_query (lines 204-246)**
Add at end before closing brace:
```c
#ifndef PHP_WIN32
    pid_t created_pid;   /* PID when query was created (fork detection) */
#endif
```

**Struct: fbird_transaction (lines 124-133)**
Add after `void *fbt_transaction;`:
```c
#ifndef PHP_WIN32
    pid_t created_pid;   /* PID when transaction was created (fork detection) */
#endif
```

**Struct: fbird_batch (lines 253-260)**
Add at end before closing brace:
```c
#ifndef PHP_WIN32
    pid_t created_pid;   /* PID when batch was created (fork detection) */
#endif
```

[Files]
Modify 5 files for struct changes, destructor fixes, and creation point updates.

**Files to modify:**

1. `php_fbird_includes.h` - Add `created_pid` field to 5 struct definitions
2. `fbird_blobs.c` - Fix destructor + set created_pid at 6 creation points
3. `fbird_events.c` - Fix destructor + set created_pid at 1 creation point
4. `fbird_query_prepare.c` - Fix destructor + set created_pid at 1 creation point
5. `fbird_query_exec.c` - Set created_pid at 2 query creation points + 2 transaction points
6. `firebird.c` - Fix 2 destructors + set created_pid at batch creation + transaction points

[Functions]
Fix 5 destructor functions and update 13 creation points with created_pid assignment.

**Destructors to fix:**

1. `_php_fbird_free_blob()` in fbird_blobs.c (lines 177-199)
   - Add NULL check, fork-safety (global + resource), MSHUTDOWN guard, master_instance validation

2. `_php_fbird_free_event_rsrc()` in fbird_events.c (lines 109-114)
   - Add NULL check, fork-safety (global + resource), MSHUTDOWN guard
   - Also update `_php_fbird_free_event()` (lines 58-107) with master_instance validation

3. `php_fbird_free_query_rsrc()` in fbird_query_prepare.c (lines 137-199)
   - Already has NULL check
   - Add fork-safety (global + resource), MSHUTDOWN guard, master_instance validation

4. `_php_fbird_free_trans()` in firebird.c (lines 995-1032)
   - Already has fork-safety global
   - Add NULL check, fork-safety resource, MSHUTDOWN guard, master_instance validation

5. `_php_fbird_free_batch()` in firebird.c (lines 1035-1071)
   - Already has fork-safety global
   - Add NULL check, fork-safety resource, MSHUTDOWN guard, master_instance validation

**Creation points to update (set created_pid = getpid()):**

fbird_blob (6 points in fbird_blobs.c):
- Line 430: fbird_blob_create()
- Line 476: fbird_blob_create_seekable()
- Line 521: fbird_blob_open()
- Line 978: fbird_blob_create_stream()
- Line 1036: fbird_blob_open_stream()
- Line 1098: fbird_blob_open_seekable()

fbird_event (1 point):
- fbird_events.c line 290: fbird_set_event_handler()

fbird_query (3 points):
- fbird_query_prepare.c line 237: _php_fbird_prepare()
- fbird_query_exec.c line 495: result clone in _php_fbird_exec()
- fbird_query_exec.c line 691: fbird_execute_immediate() result clone

fbird_transaction (3 points):
- fbird_query_exec.c line 127: auto-transaction in _php_fbird_exec()
- fbird_query_exec.c line 1602: fbird_execute_immediate() auto-transaction
- firebird.c line 2609: fbird_trans() explicit transaction

fbird_batch (1 point):
- firebird.c line 3120: fbird_batch_create()

[Classes]
No class modifications required - this is a C extension without OOP constructs.

This implementation only modifies C structs and functions. No PHP classes are affected.

[Dependencies]
No new dependencies required.

The implementation uses only existing system headers:
- `<unistd.h>` for `pid_t` and `getpid()` (already included)
- All guards use existing macros: `IBG()`, `IB_STATUS`, `FBDEBUG()`

[Testing]
Existing PHPT tests must continue to pass; no new test files required.

**Testing strategy:**

1. **Compile test**: Ensure all files compile without warnings
   ```bash
   make clean && phpize --clean && phpize && ./configure && make
   ```

2. **Unit tests**: Run full PHPT test suite
   ```bash
   make test TESTS=tests/
   ```

3. **Fork safety test**: Use manual_fork_test.php with each resource type
   ```bash
   php manual_fork_test.php
   ```

4. **PHPStan parallel**: Run PHPStan with parallel workers to trigger fork scenarios
   ```bash
   vendor/bin/phpstan analyse --memory-limit=1G
   ```

5. **Valgrind check**: Run under Valgrind to verify no memory leaks in new code paths
   ```bash
   make test TESTS="-m tests/fbird_blob_001.phpt"
   ```

**Expected behavior after fix:**
- No SIGSEGV in forked child processes
- No crashes during MSHUTDOWN when resources are orphaned
- Memory properly freed in all exit paths (efree after early returns)

[Implementation Order]
Implement changes in dependency order: header first, then creation points, then destructors.

1. **Step 1: Update php_fbird_includes.h**
   - Add `created_pid` field to all 5 structs
   - Place inside `#ifndef PHP_WIN32` guards
   - Compile test to verify struct changes

2. **Step 2: Update creation points - fbird_blobs.c**
   - Set `ib_blob->created_pid = getpid();` at 6 creation points
   - Add `#ifndef PHP_WIN32` guards around assignments

3. **Step 3: Update creation points - fbird_events.c**
   - Set `event->created_pid = getpid();` at 1 creation point

4. **Step 4: Update creation points - fbird_query_prepare.c**
   - Set `ib_query->created_pid = getpid();` at 1 creation point

5. **Step 5: Update creation points - fbird_query_exec.c**
   - Set `created_pid = getpid();` at 2 query points + 2 transaction points

6. **Step 6: Update creation points - firebird.c**
   - Set `created_pid = getpid();` for batch (1 point) + transaction (1 point)

7. **Step 7: Fix _php_fbird_free_blob() destructor**
   - Add all 5 safety guards following service destructor template

8. **Step 8: Fix _php_fbird_free_event_rsrc() destructor**
   - Add guards in rsrc function
   - Add master_instance validation in _php_fbird_free_event()

9. **Step 9: Fix php_fbird_free_query_rsrc() destructor**
   - Add 4 missing guards (already has NULL check)

10. **Step 10: Fix _php_fbird_free_trans() destructor**
    - Add 4 missing guards (already has fork-safety global)

11. **Step 11: Fix _php_fbird_free_batch() destructor**
    - Add 4 missing guards (already has fork-safety global)

12. **Step 12: Final testing**
    - Full compile test
    - Run all PHPT tests
    - Manual fork test
    - PHPStan parallel test

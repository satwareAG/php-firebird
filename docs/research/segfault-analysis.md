# Segfault Analysis - PHP Firebird Extension

## Overview

Analysis of hard-to-reproduce segfaults (exit code 139) during PHP shutdown phase.

**Related Issues:**
- Issue #55: `si_addr=0x4` crash in PHPStan parallel workers
- Issue #50/#51: SIGSEGV during MSHUTDOWN persistent connection cleanup

## Crash Pattern

```
Exit code: 139 (SIGSEGV)
Timing: AFTER PHPStan/PHPUnit finishes execution
Phase: PHP's module shutdown (MSHUTDOWN) or request shutdown (RSHUTDOWN)
```

## Root Cause Hypothesis

1. **Fork-safety guard gaps** in resource destructors
2. **Race conditions** in persistent link hash cleanup
3. **Invalid handle access** in forked child processes

## Debugging Tools

### Stress Test: `tests/stress_shutdown.phpt`

Exercises all destructor paths:
- Persistent connections (`fbird_pconnect`) → `le_plink`
- Normal connections (`fbird_connect`) → `le_link`
- Explicit transactions (`fbird_trans`) → `le_trans`
- Batches (`fbird_batch_create`) → `le_batch` (FB 4.0+)
- ReflectionExtension metadata loading
- NO explicit cleanup - forces destructor execution at shutdown

### Debug Script: `scripts/debug_segfault.sh`

```bash
# Memory error detection
./scripts/debug_segfault.sh --valgrind tests/stress_shutdown.phpt

# Crash backtrace
./scripts/debug_segfault.sh --gdb tests/stress_shutdown.phpt

# Signal/syscall tracing
./scripts/debug_segfault.sh --strace tests/stress_shutdown.phpt

# Run all tools
./scripts/debug_segfault.sh --all
```

### Docker Execution

```bash
# Enter container
docker compose exec php81-fb3-dev bash

# Run inside container
cd /ext
./scripts/debug_segfault.sh --valgrind
```

## Analysis Sessions

### Session: 2026-01-04 (Initial Investigation)

**Environment:**
- PHP version: 8.1.33
- Firebird version: 3.0 (firebird30 container)
- Container: php81-fb3-dev

**Commands Run:**
```bash
./scripts/debug_segfault.sh --valgrind tests/stress_shutdown.phpt
```

**Findings:**

**12 Memory Errors Detected - Use-After-Free in Persistent Connection Cleanup**

```
==16048== Invalid read of size 4
==16048==    at 0x6523A0: zend_hash_str_del (in /usr/local/bin/php)
==16048==    by 0x699209C: _php_fbird_close_plink (firebird.c:987)
==16048==  Address 0x72b5118 is 72 bytes inside a block of size 10,240 free'd
==16048==    at 0x484787F: free (vg_replace_malloc.c:989)
==16048==    by 0x641AE8: zend_deactivate

==16048== Invalid read of size 8
==16048==    at 0x69A3FE7: detachNoThrow (fb_connection.hpp:531)
==16048==    by 0x69A3FE7: fbc_disconnect (firebird_utils.cpp:537)
==16048==    by 0x69920CA: _php_fbird_close_plink (firebird.c:997)
==16048==  Address 0x7285c80 is 0 bytes inside a block of size 96 free'd
==16048==    at 0x484886D: operator delete(void*, unsigned long)
==16048==    by 0x6990E46: fbc_drop_database.cold (firebird_utils.cpp:573)

==16048== Invalid free() / delete / delete[] / realloc()
==16048==    at 0x484886D: operator delete(void*, unsigned long)
==16048==    by 0x69A419F: fbc_disconnect (firebird_utils.cpp:544)
==16048==    by 0x69920CA: _php_fbird_close_plink (firebird.c:997)
==16048==  Address 0x7285c80 is 0 bytes inside a block of size 96 free'd
==16048==    by 0x6990E46: fbc_drop_database.cold (firebird_utils.cpp:573)
```

**Root Cause Identification:**

1. **Sequence of Events:**
   - `fbird_drop_db()` is called during request shutdown
   - It deletes the Connection object (`fbc_drop_database.cold` at firebird_utils.cpp:573)
   - Later, during MSHUTDOWN, `_php_fbird_close_plink()` runs via `zend_hash_graceful_reverse_destroy`
   - It tries to access/delete the **already-freed** Connection object

2. **Problematic Files:**
   - `firebird.c:987` - `_php_fbird_close_plink()` calls `zend_hash_str_del` on freed memory
   - `firebird.c:997` - `_php_fbird_close_plink()` calls `fbc_disconnect` on already-deleted object
   - `firebird_utils.cpp:537-544` - `fbc_disconnect()` tries to destruct already-deleted Connection
   - `firebird_utils.cpp:573` - `fbc_drop_database.cold` - first deletion point

3. **Pattern:** Double-free / Use-after-free of `fb::Connection` object

**Conclusion:**

The test confirms "fbird_drop_db()" and persistent connection cleanup have a **dangling pointer issue**. When `fbird_drop_db()` deletes the Connection object, the persistent link hash still contains a reference to the now-invalid pointer. During MSHUTDOWN, `_php_fbird_close_plink()` tries to use this dangling pointer.

**Recommended Fix:**
- After `fbc_drop_database()` deletes the Connection, the persistent link entry must be removed from the hash table (or the Connection pointer must be NULLed out and checked)
- Alternative: Don't allow `fbird_drop_db()` on persistent connections without explicit close first

---

### Session: YYYY-MM-DD (Template)

**Environment:**
- PHP version:
- Firebird version:
- Container:

**Commands Run:**
```bash
# Commands here
```

**Findings:**
```
# Output here
```

**Conclusion:**

---

## Destructor Code Review

### Critical Functions in `firebird.c`

| Function | Resource Type | Fork Guard |
|----------|--------------|------------|
| `_php_fbird_close_link` | `le_link` | `created_pid` |
| `_php_fbird_close_plink` | `le_plink` | `created_pid` + `in_mshutdown` |
| `_php_fbird_free_trans` | `le_trans` | `created_pid` |
| `_php_fbird_free_batch` | `le_batch` | `created_pid` |

### Fork-Safety Guards

```c
// Pattern in all destructors
if (link->created_pid != getpid()) {
    // Skip cleanup - handle belongs to parent process
    return;
}
```

### MSHUTDOWN Guard

```c
// In persistent link destructor
if (IBG(in_mshutdown)) {
    // Don't access EG() - may be destroyed
    return;
}
```

## Known Crash Signatures

### NULL + Offset Pattern (Issue #55)

```
si_addr=0x4
```
Indicates `NULL->field` access where field is at byte offset 4.

**Likely locations:**
- Struct member access after failed resource lookup
- Handle dereference after cleanup

## References

- [firebird.c MSHUTDOWN](../../../firebird.c) (~line 2050)
- [valgrind.sh](../../scripts/analysis/valgrind.sh)
- [Implementation Plan](../../implementation_plan.md)

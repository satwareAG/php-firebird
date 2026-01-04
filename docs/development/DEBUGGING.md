# Debugging PHP Extension Memory Issues

This guide documents debugging techniques for PHP C extension development, with specific examples from the php-firebird extension's Issue #56 (use-after-free) investigation.

## Table of Contents

1. [Quick Reference](#quick-reference)
2. [Valgrind Memory Analysis](#valgrind-memory-analysis)
3. [GDB Crash Analysis](#gdb-crash-analysis)
4. [strace Signal Tracing](#strace-signal-tracing)
5. [Docker Container Debugging](#docker-container-debugging)
6. [Crash Signature Analysis](#crash-signature-analysis)
7. [qa.sh Debugging Flags](#qash-debugging-flags)
8. [Real-World Example: Issue #56](#real-world-example-issue-56)

---

## Quick Reference

### qa.sh Debugging Options

```bash
# Standard QA (no debugging)
./scripts/qa.sh --mode standard --container php83-dev

# With Valgrind memory checking
./scripts/qa.sh --valgrind --container php83-dev

# With AddressSanitizer
./scripts/qa.sh --asan --container php83-dev

# Stop on first failure
./scripts/qa.sh --fail-fast --container php83-dev

# Combined
./scripts/qa.sh --valgrind --fail-fast --container php83-dev
```

### Direct Debugging Commands

```bash
# Valgrind
USE_ZEND_ALLOC=0 valgrind --leak-check=full --track-origins=yes \
  php -d extension=modules/firebird.so tests/test.phpt

# GDB backtrace
gdb -batch -ex 'run' -ex 'bt 30' -ex 'x/10i $rip-20' \
  --args php -d extension=modules/firebird.so tests/test.phpt

# strace signal tracing
strace -f -e trace=signal \
  php -d extension=modules/firebird.so tests/test.phpt 2>&1
```

---

## Valgrind Memory Analysis

### Critical Environment Variables

| Variable | Value | Purpose |
|----------|-------|---------|
| `USE_ZEND_ALLOC` | `0` | Disable PHP's custom allocator, use system malloc (required for Valgrind) |
| `ZEND_DONT_UNLOAD_MODULES` | `1` | Keep extension loaded for accurate stack traces |

### Running Valgrind

```bash
# Basic memory check
USE_ZEND_ALLOC=0 valgrind --leak-check=full \
  php -d extension=modules/firebird.so tests/stress_shutdown.phpt

# Full check with origins (slowest, most detailed)
USE_ZEND_ALLOC=0 valgrind --leak-check=full --track-origins=yes -s \
  php -d extension=modules/firebird.so tests/stress_shutdown.phpt 2>&1

# Show reachable blocks (for complete leak analysis)
USE_ZEND_ALLOC=0 valgrind --leak-check=full --show-reachable=yes \
  php -d extension=modules/firebird.so tests/stress_shutdown.phpt
```

### Using scripts/analysis/valgrind.sh

```bash
# Standard run
./scripts/analysis/valgrind.sh tests/stress_shutdown.phpt

# With custom options
./scripts/analysis/valgrind.sh --track-origins=yes tests/stress_shutdown.phpt
```

### Interpreting Valgrind Output

#### Use-After-Free Pattern

```
==12345== Invalid read of size 4
==12345==    at 0x7F1234: fbird_drop_db (firebird.c:1861)
==12345==    by 0x7F5678: zif_fbird_drop_db (firebird.c:1870)
==12345==  Address 0x71f0028 is 40 bytes inside a block of size 120 free'd
==12345==    at 0x484B6F0: free (vg_replace_malloc.c:884)
==12345==    by 0x7F9ABC: _php_fbird_close_plink (firebird.c:987)
```

- **Address X is N bytes inside a block** → UAF, memory freed elsewhere
- **free'd by** → Shows what freed the memory
- **40 bytes inside** → Offset indicates which struct field was accessed

#### Memory Leak Pattern

```
==12345== 120 bytes in 1 blocks are definitely lost
==12345==    at 0x484DA83: calloc (vg_replace_malloc.c:1328)
==12345==    by 0x7F1234: _php_fbird_alloc_link (firebird.c:500)
```

### Suppression Files

Firebird/ICU libraries may report expected "leaks" (global buffers retained until exit). Use the suppression file:

```bash
USE_ZEND_ALLOC=0 valgrind --suppressions=valgrind-php.supp --leak-check=full \
  php -d extension=modules/firebird.so tests/test.phpt
```

---

## GDB Crash Analysis

### Batch Mode (Non-Interactive)

For CI/automated debugging:

```bash
gdb -batch \
  -ex 'handle SIGSEGV stop' \
  -ex 'run' \
  -ex 'bt 30' \
  -ex 'info registers' \
  -ex 'x/10i $rip-20' \
  --args php -d extension=modules/firebird.so tests/stress_shutdown.phpt
```

### Interactive GDB Session

```bash
gdb --args php -d extension=modules/firebird.so tests/test.phpt

# In GDB:
(gdb) break _php_fbird_close_plink
(gdb) run
(gdb) bt           # backtrace
(gdb) frame 2      # switch to frame
(gdb) print ib_link->hash_key
(gdb) x/10x $rsp   # examine stack
```

### Using scripts/debug_segfault.sh

```bash
# Valgrind analysis
./scripts/debug_segfault.sh --valgrind tests/stress_shutdown.phpt

# GDB backtrace
./scripts/debug_segfault.sh --gdb tests/stress_shutdown.phpt

# strace signal trace
./scripts/debug_segfault.sh --strace tests/stress_shutdown.phpt

# All tools
./scripts/debug_segfault.sh --all tests/stress_shutdown.phpt
```

### Core Dump Analysis

```bash
# Enable core dumps
ulimit -c unlimited
echo "/tmp/core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern

# Run PHP until crash
php -d extension=modules/firebird.so tests/stress_shutdown.phpt

# Analyze core
gdb php /tmp/core.php.12345
(gdb) bt full
```

---

## strace Signal Tracing

### Basic Signal Tracing

```bash
strace -f -e trace=signal php -d extension=modules/firebird.so tests/test.phpt 2>&1
```

### Interpreting SIGSEGV Output

```
--- SIGSEGV {si_signo=SIGSEGV, si_code=SEGV_MAPERR, si_addr=0x4} ---
```

| Field | Meaning |
|-------|---------|
| `si_signo=SIGSEGV` | Segmentation fault signal |
| `si_code=SEGV_MAPERR` | Address not mapped (invalid pointer) |
| `si_code=SEGV_ACCERR` | Permission denied (e.g., write to read-only) |
| `si_addr=0x4` | Address that caused the fault |

### si_addr Pattern Analysis

| `si_addr` | Likely Cause |
|-----------|--------------|
| `0x0` | Direct NULL pointer dereference |
| `0x1` - `0xFF` | NULL + small offset (accessing struct field on NULL) |
| `0x4`, `0x8` | NULL pointer + struct field offset |
| Large valid-looking | Wild/dangling pointer, heap corruption |

**Example**: `si_addr=0x4` → Accessing `((struct*)NULL)->field` where `field` is at offset 4.

---

## Docker Container Debugging

### Running Tests in Container

```bash
# Enter container
docker compose -f docker/docker-compose.yml exec php83-dev bash

# Run test with Valgrind
cd /ext
USE_ZEND_ALLOC=0 valgrind --leak-check=full \
  php -d extension=modules/firebird.so tests/stress_shutdown.phpt
```

### Running from Host

```bash
# Valgrind in container
docker compose -f docker/docker-compose.yml exec -T php83-dev bash -c \
  "USE_ZEND_ALLOC=0 valgrind --leak-check=full --track-origins=yes -s \
   php -d extension=modules/firebird.so tests/stress_shutdown.phpt 2>&1"

# GDB in container
docker compose -f docker/docker-compose.yml exec -T php83-dev bash -c \
  "gdb -batch -ex 'run' -ex 'bt 30' \
   --args php -d extension=modules/firebird.so tests/stress_shutdown.phpt"

# strace in container (requires strace package)
docker compose -f docker/docker-compose.yml exec -T php81-fb3-dev bash -c \
  "strace -f -e trace=signal \
   php -d extension=modules/firebird.so tests/stress_shutdown.phpt 2>&1"
```

### Available Debug Containers

| Container | PHP | Firebird | Use Case |
|-----------|-----|----------|----------|
| `php81-fb3-dev` | 8.1 | 3.0 | Legacy testing, includes strace |
| `php83-dev` | 8.3 | 4.0 | Standard development |
| `php84-dev` | 8.4 | 4.0 | Latest PHP |

---

## Crash Signature Analysis

### Common Crash Patterns

#### 1. NULL Pointer + Offset (Fork Safety Issue)

```
si_addr=0x4
```

**Pattern**: Accessing struct field on NULL pointer in forked child process.

**Fix**: Add NULL check in resource destructor.

```c
// WRONG
static void _php_fbird_close_link(zend_resource *rsrc)
{
    fbird_db_link *link = (fbird_db_link *)rsrc->ptr;
    if (link->created_pid != getpid()) return;  // CRASH if link==NULL
}

// CORRECT
static void _php_fbird_close_link(zend_resource *rsrc)
{
    fbird_db_link *link = (fbird_db_link *)rsrc->ptr;
    if (link == NULL) return;  // Guard first
    if (link->created_pid != getpid()) return;
}
```

#### 2. Use-After-Free in Hash Table

```
Address 0x71f0028 is 40 bytes inside a block of size 120 free'd
```

**Pattern**: `zend_hash_str_del()` triggers destructor that frees struct, then code continues using freed pointer.

**Fix**: Don't access struct after triggering destructor.

```c
// WRONG
zend_hash_str_del(&EG(persistent_list), ib_link->hash_key, strlen(ib_link->hash_key));
memset(ib_link->hash_key, 0, 16);  // UAF: ib_link freed by destructor!

// CORRECT
zend_hash_str_del(&EG(persistent_list), ib_link->hash_key, strlen(ib_link->hash_key));
// ib_link is now invalid, don't access it
```

#### 3. MSHUTDOWN EG() Access

```
Invalid read during MSHUTDOWN
```

**Pattern**: Resource destructor accesses `EG(regular_list)` or `EG(persistent_list)` during module shutdown after they've been destroyed by `zend_deactivate()`.

**Fix**: Check `IBG(in_mshutdown)` flag.

```c
if (!IBG(in_mshutdown)) {
    zend_hash_str_del(&EG(regular_list), key, strlen(key));
}
```

---

## qa.sh Debugging Flags

### --valgrind

Runs tests under Valgrind with proper environment:

```bash
./scripts/qa.sh --valgrind --container php83-dev
```

Equivalent to:
```bash
USE_ZEND_ALLOC=0 ZEND_DONT_UNLOAD_MODULES=1 valgrind --leak-check=full ...
```

### --asan (AddressSanitizer)

Runs tests with PHP compiled with AddressSanitizer:

```bash
./scripts/qa.sh --asan --container php83-dev
```

**Note**: Requires PHP built with ASAN support. Faster than Valgrind for basic memory issues.

### --fail-fast

Stops at first failure for efficient debugging:

```bash
./scripts/qa.sh --fail-fast --container php83-dev
```

### Combined Example

```bash
# Debug a specific test failure with Valgrind, stopping on first error
./scripts/qa.sh --valgrind --fail-fast --container php83-dev
```

---

## Real-World Example: Issue #56

### Symptoms

- PHPStan parallel mode caused SIGSEGV
- Exit code 139 during PHP shutdown
- Only manifested with persistent connections + `fbird_drop_db()`

### Investigation Steps

#### Step 1: Reproduce with strace

```bash
strace -f -e trace=signal php -d extension=modules/firebird.so \
  tests/stress_shutdown.phpt 2>&1 | grep -i sig
```

**Output**:
```
--- SIGSEGV {si_signo=SIGSEGV, si_code=SEGV_MAPERR, si_addr=0x71f0028} ---
```

**Analysis**: `si_addr` is a valid-looking heap address (not NULL), suggesting use-after-free.

#### Step 2: Valgrind Analysis

```bash
USE_ZEND_ALLOC=0 valgrind --leak-check=full --track-origins=yes \
  php -d extension=modules/firebird.so tests/fbird_drop_db_001.phpt 2>&1
```

**Output**:
```
==12345== Invalid read of size 4
==12345==    at 0x7F1234: fbird_drop_db (firebird.c:1861)
==12345==  Address 0x71f0028 is 40 bytes inside a block of size 120 free'd
==12345==    at 0x484B6F0: free (vg_replace_malloc.c:884)
==12345==    by 0x7F5678: _php_fbird_close_plink (firebird.c:987)
==12345==    by 0x8A1234: zend_hash_str_del (zend_hash.c:1234)
```

**Analysis**:
- `fbird_drop_db()` at line 1861 accessed freed memory
- Memory was freed by `_php_fbird_close_plink()` destructor
- Destructor was triggered by `zend_hash_str_del()`

#### Step 3: Code Review

Found the bug at `firebird.c:1859-1861`:

```c
// Line 1859: This triggers destructor that frees ib_link
zend_hash_str_del(&EG(persistent_list), ib_link->hash_key, strlen(ib_link->hash_key));

// Line 1861: UAF! ib_link was freed by destructor above
memset(ib_link->hash_key, 0, 16);
```

#### Step 4: Fix

Removed the `zend_hash_str_del()` call since persistent list cleanup is already handled during MSHUTDOWN:

```c
// Removed: zend_hash_str_del(&EG(persistent_list), ...)
// Removed: memset(ib_link->hash_key, ...)
// Persistent connections cleaned up at MSHUTDOWN
```

#### Step 5: Verification

```bash
USE_ZEND_ALLOC=0 valgrind --leak-check=full tests/fbird_drop_db_001.phpt 2>&1 | tail -5
```

**Output**:
```
==12345== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### Lessons Learned

1. **`zend_hash_str_del()` triggers destructor immediately** - Don't access hash entry after deletion
2. **Valgrind + USE_ZEND_ALLOC=0 is essential** for memory debugging
3. **strace si_addr patterns** help identify crash type (NULL vs heap corruption)
4. **MSHUTDOWN vs RSHUTDOWN timing** - EG() globals may not exist during MSHUTDOWN

---

## Additional Resources

- [PHP Internals Book - Memory Management](https://www.phpinternalsbook.com/php7/extensions_design/memory_management.html)
- [Valgrind Quick Start](https://valgrind.org/docs/manual/quick-start.html)
- [GDB Quick Reference](https://sourceware.org/gdb/current/onlinedocs/gdb/index.html)
- [PHP Extension Writing Guide](https://wiki.php.net/internals/extensions)

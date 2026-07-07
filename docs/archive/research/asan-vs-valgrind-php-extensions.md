# ASan vs Valgrind for PHP Extensions

> **Research Document** | php-firebird project  
> **Date**: 2025-12-30  
> **Status**: Complete  
> **Recommendation**: Keep ASan as primary tool, add Valgrind as complementary tool

## Executive Summary

**Question**: Is AddressSanitizer (ASan) the right tool for php-firebird, or should we switch to Valgrind?

**Answer**: **Keep ASan AND add Valgrind**. The PHP core team uses both tools in their CI infrastructure because they have complementary detection capabilities. ASan remains the right choice for fast iteration during development and fuzzing; Valgrind should be added for detecting uninitialized memory reads that ASan cannot catch.

## Performance Comparison

| Metric | AddressSanitizer | Valgrind/Memcheck |
|--------|------------------|-------------------|
| **Slowdown** | 2-4x | 20-50x |
| **Memory overhead** | ~2x | ~2x |
| **Instrumentation** | Compile-time (static) | Runtime (dynamic) |
| **Rebuild required** | Yes | No |
| **Setup complexity** | Medium (special PHP build) | Low (just run with valgrind) |

## Detection Capabilities

| Error Type | ASan | Valgrind | Winner |
|------------|------|----------|--------|
| Heap buffer overflow | ✓ | ✓ | Tie |
| Heap use-after-free | ✓ | ✓ | Tie |
| Double-free | ✓ | ✓ | Tie |
| Stack buffer overflow | ✓ | ✗ | ASan |
| Global variable overflow | ✓ | ✗ | ASan |
| Stack use-after-return | ✓* | ✗ | ASan |
| **Uninitialized reads** | ✗ | ✓ | **Valgrind** |
| Memory leak detection | ✓** | ✓ | Valgrind |
| Syscall parameter errors | ✗ | ✓ | Valgrind |

\* Requires `ASAN_OPTIONS=detect_stack_use_after_return=1`  
\*\* LeakSanitizer (LSan) often disabled for PHP due to false positives from internals

## PHP-Specific Considerations

### Zend Memory Manager

PHP uses a custom memory allocator (Zend Memory Manager) with functions like `emalloc()`, `efree()`, `ecalloc()`. Both ASan and Valgrind require special configuration to work correctly with this allocator.

### Required Environment Variables

**For ASan:**
```bash
USE_ZEND_ALLOC=0        # Bypass Zend allocator, use system malloc
USE_TRACKED_ALLOC=1     # Enable tracking for arena allocator
ASAN_OPTIONS=detect_leaks=0  # Disable LSan (false positives from PHP internals)
```

**For Valgrind:**
```bash
USE_ZEND_ALLOC=0              # Bypass Zend allocator
ZEND_DONT_UNLOAD_MODULES=1    # Keep modules loaded for accurate stack traces
```

### PHP Configure Flags

| Tool | Configure Flag | Notes |
|------|---------------|-------|
| ASan | `--enable-address-sanitizer` | Mutually exclusive with Valgrind |
| Valgrind | `--with-valgrind` | Adds Valgrind annotations |

**Important**: These flags are mutually exclusive. PHP enforces this at configure time.

## PHP Core Team Approach

From the php/php-src repository (DeepWiki analysis):

1. **Both tools are used in CI** - not either/or
2. **ASan for fast feedback** - runs on every commit, catches most memory errors quickly
3. **Valgrind for thoroughness** - runs periodically, catches uninitialized reads
4. **Official recommendation**:
   > "Using valgrind is slower at detecting invalid memory read/writes than AddressSanitizer when running large numbers of tests, but does not require rebuilding php."

## Current php-firebird Setup

### Existing ASan Infrastructure

**scripts/fuzz_asan.sh:**
- Uses `php83-asan` Docker container
- Sets `ASAN_OPTIONS=detect_leaks=0` (correct for PHP)
- Rebuilds extension with ASan instrumentation
- Runs SQL fuzzer with memory error detection

**docker/docker-compose.yml:**
- `php83-asan` service with custom `Dockerfile-asan`
- `ASAN_OPTIONS: detect_leaks=1:halt_on_error=1` configured

### What's Missing

- Valgrind support for detecting uninitialized reads
- Valgrind suppression file for known PHP false positives
- CI job for periodic Valgrind runs

## Recommendations

### 1. Keep ASan as Primary Tool ✓

**Rationale:**
- Already working and integrated
- 10-25x faster than Valgrind (crucial for fuzzing)
- Successfully found heap-use-after-free bug (Issue #32)
- Catches stack/global overflows that Valgrind misses

**No changes needed** to current ASan setup.

### 2. Add Valgrind as Secondary Tool

**Rationale:**
- Catches uninitialized memory reads (ASan blind spot)
- No rebuild required (can run against standard PHP build)
- PHP core team uses both - industry best practice

**Implementation:**

```bash
# scripts/run-valgrind.sh
#!/bin/bash
set -euo pipefail

export USE_ZEND_ALLOC=0
export ZEND_DONT_UNLOAD_MODULES=1

valgrind \
    --tool=memcheck \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --suppressions=valgrind-php.supp \
    --error-exitcode=1 \
    php run-tests.php -m tests/
```

### 3. Use Appropriate Tool for Each Scenario

| Scenario | Recommended Tool | Reason |
|----------|-----------------|--------|
| Daily development | ASan | Fast iteration |
| Fuzzing | ASan | Performance critical |
| Pre-release QA | Both | Maximum coverage |
| Debugging specific bug | Depends | ASan first, Valgrind if uninitialized read suspected |
| CI pipeline (every commit) | ASan | Speed |
| CI pipeline (nightly/weekly) | Valgrind | Thoroughness |

### 4. Valgrind Suppression File

The project already has `valgrind-php.supp`. Ensure it includes suppressions for:
- Known PHP internal allocations
- Firebird client library internals (libfbclient)
- OpenSSL/crypto library noise

## Implementation Plan

### Phase 1: Valgrind Script (Quick Win)
- [ ] Create `scripts/run-valgrind.sh`
- [ ] Verify `valgrind-php.supp` is up-to-date
- [ ] Document usage in CONTRIBUTING.md

### Phase 2: CI Integration
- [ ] Add Valgrind job to CI (weekly schedule)
- [ ] Configure to run subset of tests (performance)
- [ ] Set up artifact collection for Valgrind reports

### Phase 3: Documentation
- [ ] Update README.md with both tool options
- [ ] Add troubleshooting guide for memory errors
- [ ] Document when to use which tool

## Conclusion

**ASan is the right tool** for php-firebird's primary use case (fast iteration during development and fuzzing). The heap-use-after-free bug found by ASan in Issue #32 validates this choice.

**Valgrind should be added** as a complementary tool to catch uninitialized memory reads - a class of bugs that ASan cannot detect. This follows the PHP core team's approach of using both tools.

The 20-50x performance overhead of Valgrind makes it unsuitable for fuzzing, but valuable for periodic thorough testing.

## References

1. [AddressSanitizer Documentation](https://clang.llvm.org/docs/AddressSanitizer.html)
2. [Valgrind User Manual](https://valgrind.org/docs/manual/manual.html)
3. [PHP QA - Running Tests](https://qa.php.net/running-tests.php)
4. [php/php-src CI Configuration](https://github.com/php/php-src/blob/master/.github/workflows/)
5. [Google Sanitizers Wiki](https://github.com/google/sanitizers/wiki)
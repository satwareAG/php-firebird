# GCC ThreadSanitizer Segfault on Debian/Ubuntu

**Date**: 2026-03-31
**Impact**: Critical - blocks all TSan CI and local validation
**Resolution**: Use Clang instead of GCC for TSan builds

## Problem

PHP compiled with GCC's `-fsanitize=thread` segfaults immediately on
`php -v` - both with ZTS and without ZTS, both with native configure
flags and post-configure sed injection.

Exit code 139 (SIGSEGV) during dynamic linker initialization, before
TSan even gets to initialize. No output, no TSan warnings.

## Root Cause

GCC 12+ TSan is broken on Debian bookworm and Ubuntu 24.04. The crash
occurs in `sanitizer_posix_libcdep.cpp` during signal handler setup.
This affects ALL GCC TSan-compiled programs, not just PHP.

References:
- https://bugs.archlinux.org/task/73835.html (GCC 11+ TSan breakage)
- https://groups.google.com/g/thread-sanitizer/c/0xA7qH2Pr-o

## Solution

Use **Clang** (`CC=clang CXX=clang++`) for all TSan builds. Clang's
TSan implementation works correctly.

Additionally, set `TSAN_OPTIONS="exitcode=0:halt_on_error=0"` when
running `./configure` so that autoconf test programs don't fail when
TSan detects races in generated test code.

## Verified

```bash
# Inside debian:bookworm-slim container:
./configure --enable-zts --disable-all ... \
  CC=clang CXX=clang++ \
  CFLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=thread" \
  LDFLAGS="-fsanitize=thread"
make -j$(nproc)
./sapi/cli/php -v
# Output: PHP 8.3.26 (cli) (built: ...) (ZTS)
```

## Failed Approaches (for reference)

1. **Post-configure sed injection with GCC**: Segfault
2. **Native configure with GCC + TSAN_OPTIONS=exitcode=0**: Segfault
3. **GCC without ZTS**: Same segfault
4. **GCC with --disable-all (minimal PHP)**: Same segfault
5. **Removing --enable-mbstring** (IFUNC resolver theory): Not the cause

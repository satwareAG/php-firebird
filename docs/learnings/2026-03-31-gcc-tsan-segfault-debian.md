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

## Docker BuildKit: Missing libclang-rt-dev

When building PHP with Clang + TSan inside Docker BuildKit, `make`
fails with exit 2 (linker error) if only the `clang` package is
installed. On Debian bookworm, `clang` does NOT include the compiler-rt
TSan runtime library (`libclang_rt.tsan-x86_64.a`).

**Symptom**: Compile steps succeed (Clang inserts `__tsan_*` calls
into .o files without the runtime), but the link step fails because
`-fsanitize=thread` in LDFLAGS tells Clang to link with the TSan
runtime which doesn't exist.

**Fix**: Install `libclang-rt-dev` alongside `clang`:

```dockerfile
RUN apt-get install -y --no-install-recommends \
    clang \
    libclang-rt-dev \
    ...
```

**Why CI works without it**: The GitHub Actions CI runs on ubuntu-24.04
bare metal where `clang` + `llvm` packages pull in compiler-rt
transitively, or the system has it pre-installed.

**Docker BuildKit two-phase approach** (required because BuildKit's
restricted seccomp profile blocks TSan-compiled test programs):

1. Configure WITHOUT `-fsanitize=thread` (avoids configure exit 77)
2. Post-configure: inject TSan via `sed` into Makefile's CFLAGS_CLEAN,
   LDFLAGS, and EXTRA_LDFLAGS
3. `make -j$(nproc)` succeeds with `libclang-rt-dev` installed

Verified: PHP 8.3.26 (ZTS DEBUG) with Thread Safety enabled, built
inside Docker BuildKit on 2026-03-31.

## Failed Approaches (for reference)

1. **Post-configure sed injection with GCC**: Segfault
2. **Native configure with GCC + TSAN_OPTIONS=exitcode=0**: Segfault
3. **GCC without ZTS**: Same segfault
4. **GCC with --disable-all (minimal PHP)**: Same segfault
5. **Removing --enable-mbstring** (IFUNC resolver theory): Not the cause
6. **Clang in Docker BuildKit without libclang-rt-dev**: make exit 2 (linker error)

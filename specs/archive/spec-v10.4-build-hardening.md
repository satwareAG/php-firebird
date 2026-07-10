---
description: >-
  v10.4.x build hardening: compiler security flags, C17 standard pinning,
  pointer smuggling elimination, LTO for release builds.
tags: [build-system, security, compiler, lto, v10.4]
priority: 5
---

> **Status: RELEASED** - Shipped across v10.4.x (commit 99c7c0f), included in v10.6.2

# Spec: v10.4.x Build Hardening

## Goal

Harden the build system with modern compiler flags, explicit C standard selection,
and elimination of unsafe internal patterns.

## Success Criteria

- [x] H4: config.m4 adds `-Wall -Wextra -D_FORTIFY_SOURCE=2 -fstack-protector-strong`
- [x] M1: C sources compile with explicit `-std=gnu17`
- [x] M8: Connection pointer passed via struct field instead of status vector abuse
- [x] L1: Release builds use `-flto=auto` for LTO optimization
- [x] Zero new compiler warnings after flag addition
- [x] All 274 tests pass with new flags

## Part 1: H4 - Compiler Hardening Flags

### Fix

Add to `config.m4` after `PHP_NEW_EXTENSION`:
```m4
PHP_CHECK_GCC_ARG(-Wall, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -Wall"])
PHP_CHECK_GCC_ARG(-Wextra, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -Wextra"])
PHP_CHECK_GCC_ARG(-Wformat-security, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -Wformat-security"])
PHP_CHECK_GCC_ARG(-fstack-protector-strong, [FIREBIRD_CFLAGS="$FIREBIRD_CFLAGS -fstack-protector-strong"])
CFLAGS="$CFLAGS $FIREBIRD_CFLAGS -D_FORTIFY_SOURCE=2"
```

### Affected Files

- `config.m4`

## Part 2: M1 - C Standard Pinning

### Fix

Add `-std=gnu17` to C compilation flags. This ensures consistent behavior across
GCC 7+ and Clang 5+ build environments.

### Affected Files

- `config.m4`

## Part 3: M8 - Pointer Smuggling Elimination

### Current State

`fbird_connection.c:249` stores a `void*` connection pointer in `IBG(status[ISC_STATUS_LENGTH-1])` to pass between `_php_fbird_attach_db()` and `_php_fbird_connect()`.

### Fix

Add `void **out_connection` parameter to `_php_fbird_attach_db()` instead of
abusing the status vector.

### Affected Files

- `fbird_connection.c` - `_php_fbird_attach_db()` signature and callers
- `php_fbird_connection.h` - update prototype

## Part 4: L1 - LTO for Release Builds

### Fix

Add `-flto=auto` to `scripts/build-precompiled.sh` CFLAGS for release builds.
Do not enable in `config.m4` (user builds may have incompatible toolchains).

### Affected Files

- `scripts/build-precompiled.sh`
- `.github/workflows/release-linux.yml` - CFLAGS

## Risks

| Risk | Mitigation |
|------|------------|
| `-Wall -Wextra` may surface warnings in existing code | Fix all warnings before merging |
| `-D_FORTIFY_SOURCE=2` requires `-O1` minimum | Release builds already use `-O2` |
| LTO increases build time | Only in release pipeline, not user builds |

## Test Strategy

1. Full CI matrix with new flags - zero warnings
2. ASAN + UBSan with new flags
3. Benchmark precompiled build with/without LTO

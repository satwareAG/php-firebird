---
description: >-
  v10.6 platform expansion: ARM64/aarch64 Linux precompiled builds,
  Alpine/musl-libc builds, macOS arm64 + x86_64 binary distribution.
tags: [platform, arm64, musl, macos, x86_64, v10.6]
priority: 9
---

> **Status: COMPLETED** - Released v10.6.1 (2026-04-02) - 48 precompiled bundles across 5 platforms

# Spec: v10.6 Platform Expansion

## Goal

Expand precompiled binary distribution to ARM64 Linux, Alpine/musl-libc, and
macOS platforms to cover growing server deployments.

## Success Criteria

- [x] H7: ARM64 Linux precompiled `.so` bundles in GitHub Releases (PR #200, merged)
- [x] Alpine/musl-libc precompiled `.so` bundles (PR #201, merged)
- [x] macOS arm64 precompiled bundles (PR #203, merged)
- [x] macOS x86_64 precompiled bundles (v10.6.1, manual Intel Mac build) - `scripts/build-macos-x86_64.sh`
- [x] All bundles pass `scripts/verify-bundle.sh`

## Part 1: H7 - ARM64 Linux Builds ✅

**Implemented** (PR #200): Native GitHub ARM runners (`ubuntu-24.04-arm`) with
`manylinux_2_28_aarch64` containers. Matrix generates x86_64 + aarch64 entries
with arch-specific `runner`, `container`, `fb_arch` fields. Outputs 16 bundles
(4 PHP × 2 variants × 2 archs).

### Affected Files (Modified)

- `.github/workflows/release-linux.yml` - ARM64 matrix entries added

## Part 2: Alpine/musl-libc Builds ✅

**Implemented** (PR #201): `musllinux_1_2_x86_64` containers with Firebird
built from source. 8 musl bundles (4 PHP × 2 variants × x86_64).
musl+aarch64 excluded (GitHub JS Actions incompatible with Alpine on ARM64).

### Affected Files

- `.github/workflows/release-linux.yml` - musl matrix entry
- `docker/manylinux/` - Alpine/musl Dockerfile

## Part 3: macOS arm64 Precompiled Builds ✅

**Implemented** (PR #203): `macos-14` (arm64) runners with Firebird SDK
extracted from `.pkg`. 8 macOS arm64 bundles (4 PHP × 2 variants).

### Affected Files

- `.github/workflows/release-macos.yml` (new)
- `scripts/build-precompiled.sh` - macOS support

## Part 4: macOS x86_64 Precompiled Builds ✅

**Implemented** (v10.6.1, 2026-04-02): Manual build on Intel Mac
(`MWsatwareAG@100.64.0.12`, macOS Sonoma). GitHub Actions dropped `macos-13`
(Intel) runners, so x86_64 builds are produced via `scripts/build-macos-x86_64.sh`
and uploaded to releases with `gh release upload --clobber`.

**Key fix**: macOS SDK `globals.h` defines `#define xmlFree(ptr) free(ptr)` which
conflicts with PHP 8.4+ DOM `attr.c:208`. Resolved by using Homebrew libxml2
(`--with-libxml-dir` + `PKG_CONFIG_PATH`) so Homebrew headers take precedence
over macOS SDK headers.

8 bundles produced: PHP 8.2-8.5 × NTS/ZTS × x86_64.

### Affected Files

- `scripts/build-macos-x86_64.sh` (new) - self-contained Intel Mac build script

## Risks

| Risk | Mitigation |
|------|------------|
| Firebird ARM64 client libs unavailable | Build from source or use embedded client |
| musl incompatibility with fbclient | Test with static linking |
| macOS code signing requirements | Use ad-hoc signing for open-source |

## Test Strategy

1. ARM64 bundle loads in PHP on aarch64 Linux
2. musl bundle loads in PHP on Alpine
3. macOS arm64 bundle loads on Apple Silicon
4. macOS x86_64 bundle loads on Intel Mac (verified via `lipo -info`)

---
description: >-
  v10.6.0 platform expansion: ARM64/aarch64 Linux precompiled builds,
  Alpine/musl-libc builds, macOS universal binary distribution.
tags: [platform, arm64, musl, macos, v10.6]
priority: 9
---

> **Status: COMPLETED** - Released v10.6.0 (2026-04-01) - 40 precompiled bundles across 4 platforms

# Spec: v10.6.0 Platform Expansion

## Goal

Expand precompiled binary distribution to ARM64 Linux, Alpine/musl-libc, and
macOS platforms to cover growing server deployments.

## Success Criteria

- [x] H7: ARM64 Linux precompiled `.so` bundles in GitHub Releases (PR #200, merged)
- [x] Alpine/musl-libc precompiled `.so` bundles (PR #201, merged)
- [x] macOS arm64 precompiled bundles (PR #203, merged) - x86_64 deferred (macos-13 EOL)
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

## Part 3: macOS Precompiled Builds ✅

**Implemented** (PR #203): `macos-14` (arm64) runners with Firebird SDK
extracted from `.pkg`. 8 macOS arm64 bundles (4 PHP × 2 variants).
macOS x86_64 deferred due to `macos-13` runner deprecation.

### Affected Files

- `.github/workflows/release-macos.yml` (new)
- `scripts/build-precompiled.sh` - macOS support

## Risks

| Risk | Mitigation |
|------|------------|
| Firebird ARM64 client libs unavailable | Build from source or use embedded client |
| musl incompatibility with fbclient | Test with static linking |
| macOS code signing requirements | Use ad-hoc signing for open-source |

## Test Strategy

1. ARM64 bundle loads in PHP on aarch64 Linux
2. musl bundle loads in PHP on Alpine
3. macOS bundle loads on both Intel and Apple Silicon

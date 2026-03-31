---
description: >-
  v10.6.0 platform expansion: ARM64/aarch64 Linux precompiled builds,
  Alpine/musl-libc builds, macOS universal binary distribution.
tags: [platform, arm64, musl, macos, v10.6]
priority: 9
---

> **Status: OPEN** - Milestone v10.6.0

# Spec: v10.6.0 Platform Expansion

## Goal

Expand precompiled binary distribution to ARM64 Linux, Alpine/musl-libc, and
macOS platforms to cover growing server deployments.

## Success Criteria

- [ ] H7: ARM64 Linux precompiled `.so` bundles in GitHub Releases
- [ ] Alpine/musl-libc precompiled `.so` bundles
- [ ] macOS universal binary (x86_64 + arm64) precompiled bundles
- [ ] All bundles pass `scripts/verify-bundle.sh`

## Part 1: H7 - ARM64 Linux Builds

Use QEMU emulation or native ARM64 runners in `release-linux.yml`.
Target `manylinux_2_28` aarch64 with bundled Firebird client libraries.

### Affected Files

- `.github/workflows/release-linux.yml` - ARM64 matrix entry
- `docker/manylinux/` - ARM64 Dockerfile
- `scripts/build-precompiled.sh` - cross-compile support

## Part 2: Alpine/musl-libc Builds

Build against `musllinux_1_2` for Alpine Linux users. Requires static linking
of Firebird client or musl-compatible shared libraries.

### Affected Files

- `.github/workflows/release-linux.yml` - musl matrix entry
- `docker/manylinux/` - Alpine/musl Dockerfile

## Part 3: macOS Precompiled Builds

Build universal binary (x86_64 + arm64) on macOS GitHub runners with Homebrew
Firebird client. Distribute as `.dylib` bundles.

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

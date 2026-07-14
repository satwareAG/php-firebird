---
description: >-
  v10.4.0 supply chain hardening: artifact signing with SLSA attestation,
  SBOM generation with Syft, Dependabot for actions, version stamp automation.
tags: [supply-chain, sbom, signing, dependabot, v10.4]
priority: 3
---

> **Status: RELEASED** - Shipped in v10.4.x, included in v10.6.2

# Spec: v10.4.0 Supply Chain Hardening

## Goal

Add supply chain security measures to release pipelines following SLSA and NIS2
requirements for distributed precompiled binaries.

## Success Criteria

- [x] H1: All release artifacts have GitHub SLSA provenance attestations
- [x] H2: CycloneDX SBOM generated for each precompiled bundle
- [x] H3: Dependabot auto-updates SHA-pinned GitHub Actions weekly
- [x] M4: Release script validates version stamps in stubs match VERSION file
- [x] Users can verify artifact authenticity via `gh attestation verify`

## Part 1: H1 - Artifact Signing

### Current State

Release workflows generate SHA256 checksums but no cryptographic signatures.
No Cosign, Sigstore, or GitHub attestation exists.

### Fix

Add `gh attestation generate` step after artifact upload in both
`release-linux.yml` and `release-windows.yml`. This uses GitHub's built-in
SLSA provenance (free for public repos, requires `id-token: write` permission).

### Affected Files

- `.github/workflows/release-linux.yml` - add attestation job
- `.github/workflows/release-windows.yml` - add attestation job

## Part 2: H2 - SBOM Generation

### Fix

Add `anchore/syft` step to release workflows generating CycloneDX JSON for each
precompiled bundle. The SBOM must list bundled Firebird libraries (libfbclient,
libicu*, libtommath, libtomcrypt, libre2).

### Affected Files

- `.github/workflows/release-linux.yml` - add syft step
- `.github/workflows/release-windows.yml` - add syft step

## Part 3: H3 - Dependabot

### Fix

Create `.github/dependabot.yml` for weekly GitHub Actions ecosystem updates.

### Affected Files

- `.github/dependabot.yml` (new file)

## Part 4: M4 - Version Stamp Automation

### Current State

Stubs have stale `@version` tags (`9.0.0`, `10.0.0`) instead of current `10.3.9`.

### Fix

Add CI check script `scripts/check-version-stamps.sh` that validates `@version`
in stubs matches `VERSION` file. Add to pre-release workflow.

### Affected Files

- `scripts/check-version-stamps.sh` (new)
- `stubs/firebird-stubs.php` - update `@version`
- `stubs/firebird-classes.php` - update `@version`
- `stubs/pdo-fbird-stubs.php` - update `@version`

## Risks

| Risk | Mitigation |
|------|------------|
| Attestation requires `id-token: write` | Already scoped per-job in release workflows |
| Syft may miss statically-linked libs | Verify SBOM completeness manually for first release |

## Test Strategy

1. Verify `gh attestation verify` works on test artifacts
2. Validate SBOM contains all bundled Firebird libraries
3. Confirm Dependabot creates PRs for pinned action updates

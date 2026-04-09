---
description: >-
  v11.1.0 quality hardening: CI/CD security (script pinning, Composer install, workflow permissions),
  VERSION/VERSION.txt consistency, C code quality (bounds check, error reporting, zend_bool),
  M3 completion (fbird_batch_create OOP, orphaned header, struct unification),
  stubs completeness, Docker/CI cleanup, documentation accuracy, PDO PAGE_BUFFERS.
tags: [security, ci-cd, quality, stubs, documentation, pdo, v11.1]
priority: 12
---

> **Status: ACTIVE** — v11.1.0 quality hardening targeting QA audit P1-P2 findings.

# Spec: v11.1.0 Quality Hardening

## Goal

Eliminate supply-chain risks, close correctness gaps in C code, complete the M3
resource-to-object migration for the final `fbird_batch_create` outlier, ensure
stubs and bootstrap fully enumerate all registered constants, and clean up stale
documentation and CI/CD configuration.

No breaking changes. All improvements are backward-compatible.

## Version

`v11.1.0` — minor release off `satware-main`

## Priority

**P1** (Correctness/Security) and **P2** (Quality)

---

## Non-Functional Requirements

- **Zero supply-chain risks**: Every script fetched in CI must be pinned to an immutable
  reference (commit SHA or verified hash). No `curl | bash` without checksum verification.
- **Static analysis must block merges**: `clang-tidy` findings must fail CI, not silently pass.
- **VERSION file is single source of truth**: One canonical `VERSION.txt`; all workflows read it.
- **All registered constants are reflected in stubs**: `php --re fbird` must match stubs exactly.

---

## Success Criteria

### QH-1: CI/CD Security Hardening (P1)

#### QH-1a: IBSurgeon install script pinned (I1)
- [ ] `.github/workflows/codeql.yml` fetches `fb_vanilla-50.sh` from a pinned commit SHA (not `refs/heads/main`)
- [ ] Script mirrored to `.github/scripts/fb_install.sh` OR SHA256 checksum verified after download
- [ ] `|| true` removed from install step; failure causes workflow failure
- [ ] Pin target documented in a comment (e.g. `# pinned 2026-04-09, IBSurgeon firebirdlinuxinstall@<sha>`)

#### QH-1b: Composer install via setup-php action (I2)
- [ ] `ci.yml`, `coverage.yml`, `sanitizers.yml` replaced curl-pipe Composer install with `shivammathur/setup-php` (already used in `code-quality.yml`)
- [ ] No `curl -sS https://getcomposer.org/installer | php` pattern remains in any workflow
- [ ] Composer version is deterministic across all workflows

#### QH-1c: Windows workflow permissions scoped (I3)
- [ ] `release-windows.yml` `contents: write` moved from workflow-level to the `release` job only
- [ ] `build` job has no write permissions
- [ ] Workflow-level `permissions: read-all` (or minimal set) declared

### QH-2: VERSION File Consistency (I4/I5)

- [ ] `release-linux.yml` and `release-macos.yml` write to `VERSION.txt` (not `VERSION`)
- [ ] All 4 release workflows reference `VERSION.txt` consistently
- [ ] Fallback `"7.0.0"` in all release workflows replaced with `exit 1` (or `"11.0.0"` + warning)
- [ ] `config.m4` version injection path verified: reads `VERSION.txt` → `git describe` → `exit 1`
- [ ] Comment in each workflow documents the canonical filename: `# canonical: VERSION.txt`

### QH-3: C Code Quality (P2)

#### QH-3a: Verbose flag bounds check ordering (C2)
- [ ] `fbird_service.c:521-527` reordered: bounds check executes BEFORE `buf[spb_len++] = isc_spb_verbose`
- [ ] Off-by-one NUL terminator corruption is eliminated
- [ ] Existing service-verbose tests pass without modification

#### QH-3b: Silent RETURN_FALSE sites fixed (C6)
- [ ] `fbird_connection.c:624` — `IBG(default_link) == NULL` path adds `php_error_docref()` before `RETURN_FALSE`
- [ ] `fbird_connection.c:630` — `_php_fbird_res_from_zval()` NULL path adds error before `RETURN_FALSE`
- [ ] `fbird_transaction.c:323` — `!ib_link` path adds `php_error_docref()` before `RETURN_FALSE`
- [ ] `fbird_service.c:329` — `fbird_service_detach` resolution failure adds `FBIRD_SVC_ERROR()` before `RETURN_FALSE`
- [ ] All 4 sites emit a PHP Warning (not fatal error) to preserve backward compatibility

#### QH-3c: `zend_bool` → `bool` migration (C7)
- [ ] All 13 `zend_bool` usages replaced with `bool` in:
  - `fbird_connection.c`
  - `fbird_metadata.c`
  - `fbird_service.c`
  - `fbird_classes_internal.h` (if kept)
  - `php_fbird_includes.h`
- [ ] `grep -rn zend_bool ./*.c ./*.h` returns zero results

### QH-4: M3 Migration Completion (P2)

#### QH-4a: `fbird_batch_create` OOP migration (C3)
- [ ] `fbird_batch.c:180` replaced: `RETVAL_RES(zend_register_resource(...))` → `fbird_setup_batch_object()`
- [ ] `fbird_setup_batch_object()` from `fbird_classes.c` is called correctly with the batch handle
- [ ] `fbird_batch_create()` return type is `Firebird\Batch` object (not resource)
- [ ] Stubs updated: `fbird_batch_create` return type changed from `mixed` to `Firebird\Batch|false`
- [ ] All `fbird_batch_*` tests updated to use `instanceof Firebird\Batch` instead of `is_resource()`
- [ ] Zero `RETVAL_RES` or `zend_register_resource` calls remain in the codebase

#### QH-4b: Orphaned `fbird_classes_internal.h` removed (C4)
- [ ] `fbird_classes_internal.h` deleted from repository
- [ ] `git grep fbird_classes_internal` returns zero hits in `.c` files (it was never included)
- [ ] No struct definition loss: confirm all struct definitions exist in their canonical `.c` files
- [ ] Header file inventory updated in any documentation referencing it

#### QH-4c: Service struct unification (C5)
- [ ] `fbird_service` (defined in `fbird_service.c:17`) and `fbird_service_rsrc` (defined in `fbird_classes.c:868`) unified into single canonical struct
- [ ] Shared struct moved to a new header (e.g. `fbird_service_types.h`) included by both files
- [ ] Raw pointer casts between the two types eliminated
- [ ] Service OOP tests pass without modification

### QH-5: Stubs Completeness (P1/P2)

#### QH-5a: `FBIRD_VER` constant corrected (S1)
- [ ] `stubs/firebird-stubs.php` updated: `const FBIRD_VER = 100` (was `90`)
- [ ] `phpstan/fbird.stub.php` updated: `const FBIRD_VER = 100` (was `10`)
- [ ] `phpstan/fbird-bootstrap.php` updated: `define('FBIRD_VER', 100)` (was `10`)
- [ ] `php -r "var_dump(FBIRD_VER);"` → `int(100)` matches stubs

#### QH-5b: `FBIRD_EXCEPTION_MODE_COMPAT` added to stubs (S2)
- [ ] `stubs/firebird-stubs.php` declares `const FBIRD_EXCEPTION_MODE_COMPAT = 0`
- [ ] `phpstan/fbird.stub.php` declares `const FBIRD_EXCEPTION_MODE_COMPAT = 0`
- [ ] `check-stubs-sync.sh` still passes (89/89 + new constant)

#### QH-5c: Bootstrap constants completed (S3)
- [ ] `phpstan/fbird-bootstrap.php` adds all missing constants present in stubs:
  - `FBIRD_EXCEPTION_MODE_SILENT`, `FBIRD_EXCEPTION_MODE_THROW`, `FBIRD_FETCH_DATE_OBJ`
  - All `FBIRD_BKP_*` backup constants
  - All `FBIRD_RES_*` restore constants
  - All `FBIRD_PRP_*` properties constants
  - All `FBIRD_RPR_*` repair constants
  - All `FBIRD_STS_*` statistics constants
  - All `FBIRD_SVC_*` service constants
- [ ] PHPStan analysis at level 8 sees all service constants without "undefined constant" errors

### QH-6: Docker and CI Cleanup (P2)

#### QH-6a: `clang-tidy` blocks merges (I6)
- [ ] `continue-on-error: true` removed from the clang-tidy step in `code-quality.yml`
- [ ] Existing clang-tidy findings (if any) resolved or suppressed per-file with justification comments
- [ ] clang-tidy configuration covers: `bugprone-*`, `cert-*`, `security-*` checks

#### QH-6b: Dockerfile base image pinned, PHP 8.1 removed (I7)
- [ ] `docker/Dockerfile` `FROM debian:bookworm` pinned to SHA256 digest: `FROM debian:bookworm@sha256:<current>`
- [ ] PHP 8.1 (`8.1.33`) removed from `ARG php_vers` list
- [ ] PHP 8.5.0RC2 replaced with stable 8.5.x release (or removed until stable ships)
- [ ] Comment documents when base image digest was last updated

#### QH-6c: `qa.sh` matrix references updated (I8)
- [ ] `qa.sh` `php81-fb3-dev` service reference removed or replaced with current minimum (php82)
- [ ] `docker/docker-compose.yml` service names match what `qa.sh` references
- [ ] Running `bash qa.sh matrix` completes without "no such service" Docker error

### QH-7: Documentation Accuracy (P2)

#### QH-7a: PHP 8.1 EOL date corrected (D6)
- [ ] `README.md:941` EOL date changed from "November 25, 2025" to "November 25, 2024"
- [ ] `README.md:1004` EOL date changed from "November 25, 2025" to "November 25, 2024"
- [ ] `CHANGELOG.md:656` EOL date changed from "November 25, 2025" to "November 25, 2024"
- [ ] Cross-reference: `docs/MIGRATION-v10-to-v11.md:8` already says 2024 — confirm unchanged

#### QH-7b: Analysis docs updated (D8/D9)
- [ ] `docs/CRITICAL_ANALYSIS_2026.md` receives a `## Status as of v11.0.0` section at the top noting that resource-to-object migration is complete (Medium finding resolved)
- [ ] `docs/VERSIONING_STRATEGY.md` Phase 2 status updated from "Planned (v11.0)" to "IMPLEMENTED (v11.0.0, 2026-04-09)"

### QH-8: PDO PAGE_BUFFERS Attribute (S4)

- [ ] `pdo_fbird_driver.c` `setAttribute` switch adds `case PDO_FBIRD_ATTR_PAGE_BUFFERS:` handler
- [ ] `getAttribute` switch adds corresponding `case PDO_FBIRD_ATTR_PAGE_BUFFERS:` getter
- [ ] Attribute value is passed to Firebird connection API (`isc_dpb_page_buffers` DPB item)
- [ ] Test `pdo_fbird_page_buffers.phpt` verifies set/get round-trip

---

## User Stories

### US-QH-1: Supply chain safety (QH-1)
> As a project maintainer running CodeQL analysis, I need the Firebird install script
> to be fetched from an immutable reference so that a compromised upstream cannot inject
> malicious code into our CI environment. A mutable `refs/heads/main` script is a
> medium-high supply chain risk per SLSA Level 1.

### US-QH-2: Predictable release artifacts (QH-2)
> As a release engineer, when `VERSION.txt` is present I need all four release workflows
> to read it. A fallback of "7.0.0" means a misconfiguration silently ships artifacts
> tagged four major versions behind, making them indistinguishable from legitimate v7 builds.

### US-QH-3: Silent failures are debuggable (QH-3b)
> As a PHP developer, when `fbird_commit($txn)` returns `false` I need a PHP warning
> in the error log explaining *why* it failed. Silent false returns force users to
> debug with strace or source inspection.

### US-QH-4: Complete OOP migration (QH-4a)
> As a developer using the Batch API, the last remaining `is_resource()` check against
> `fbird_batch_create()` return value should fail, and `instanceof Firebird\Batch` should
> succeed — consistent with all other Firebird object types.

### US-QH-5: PHPStan sees all constants (QH-5)
> As a developer running PHPStan at level 8 on code that uses service backup constants
> (FBIRD_BKP_*, FBIRD_RPR_*, FBIRD_SVC_*), I should not see "undefined constant" errors
> because these are missing from the bootstrap file.

### US-QH-6: PDO attribute round-trip (QH-8)
> As a developer setting `$pdo->setAttribute(PDO_FBIRD_ATTR_PAGE_BUFFERS, 256)`, I expect
> the attribute to actually configure the Firebird page buffer pool and be retrievable
> via `getAttribute`. Currently the attribute is registered but silently discarded.

---

## Affected Files

| File | Change | Priority |
|------|--------|----------|
| `.github/workflows/codeql.yml` | Pin IBSurgeon script SHA | P1 |
| `.github/workflows/ci.yml` | Replace curl-pipe Composer | P1 |
| `.github/workflows/coverage.yml` | Replace curl-pipe Composer | P1 |
| `.github/workflows/sanitizers.yml` | Replace curl-pipe Composer | P1 |
| `.github/workflows/release-windows.yml` | Scope permissions | P1 |
| `.github/workflows/release-linux.yml` | Standardize VERSION.txt | P1 |
| `.github/workflows/release-macos.yml` | Standardize VERSION.txt | P1 |
| `.github/workflows/code-quality.yml` | Remove clang-tidy continue-on-error | P2 |
| `fbird_service.c` | Bounds check order (line 521), error reporting (line 329), struct unification | P1/P2 |
| `fbird_connection.c` | Error reporting before RETURN_FALSE (lines 624, 630) | P2 |
| `fbird_transaction.c` | Error reporting before RETURN_FALSE (line 323) | P2 |
| `fbird_metadata.c` | zend_bool → bool | P2 |
| `php_fbird_includes.h` | zend_bool → bool | P2 |
| `fbird_batch.c` | Call fbird_setup_batch_object() (line 180) | P2 |
| `fbird_classes_internal.h` | Delete file | P2 |
| `fbird_classes.c` | Service struct unification | P2 |
| `stubs/firebird-stubs.php` | FBIRD_VER=100, add FBIRD_EXCEPTION_MODE_COMPAT | P1 |
| `phpstan/fbird.stub.php` | FBIRD_VER=100, add FBIRD_EXCEPTION_MODE_COMPAT | P1 |
| `phpstan/fbird-bootstrap.php` | FBIRD_VER=100, add all missing service constants | P1 |
| `pdo_fbird_driver.c` | Handle PDO_FBIRD_ATTR_PAGE_BUFFERS | P2 |
| `docker/Dockerfile` | Pin base image, remove PHP 8.1 | P2 |
| `scripts/qa.sh` | Remove php81-fb3-dev reference | P2 |
| `README.md` | Fix PHP 8.1 EOL dates (lines 941, 1004) | P2 |
| `CHANGELOG.md` | Fix PHP 8.1 EOL date (line 656) | P2 |
| `docs/CRITICAL_ANALYSIS_2026.md` | Add v11.0.0 status section | P2 |
| `docs/VERSIONING_STRATEGY.md` | Phase 2 → IMPLEMENTED | P2 |

---

## Risks

| Risk | Mitigation |
|------|------------|
| clang-tidy findings block CI unexpectedly | Audit findings before removing continue-on-error; fix or suppress each with justification |
| Service struct unification breaks binary compatibility | Not exported API; internal types only — no ABI impact |
| Removing PHP 8.1 from Dockerfile breaks contributor workflows | v7.2.0 already dropped PHP 8.1 support; README update confirms minimum PHP 8.2 |
| FBIRD_VER value change confuses version comparisons | Value was always wrong; 100 is the correct internal representation |
| fbird_batch_create OOP change is breaking for resource-check callers | Covered by M3 migration guide; Firebird\Batch is instanceof-checkable |

## Test Strategy

1. `QH-3b`: Each RETURN_FALSE site gets a unit test verifying `error_get_last()` after the call
2. `QH-4a`: `fbird_batch_oop_001.phpt` asserting `instanceof Firebird\Batch`
3. `QH-5`: `phpstan-bootstrap-completeness.phpt` or CI script diff check
4. `QH-8`: `pdo_fbird_page_buffers.phpt` with setAttribute/getAttribute round-trip
5. All existing tests must continue to pass (no regressions)

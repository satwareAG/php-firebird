---
description: >-
  v12.1.0 Doctrine & amicron-platform compatibility: verify that php-firebird
  v12.0.0+ works with doctrine-firebird-driver v3.14.0 (customer production pin)
  and amicron-platform release/3.0.0-rc.2 (Symfony 7.4 + PHP 8.4 + FB 3.0.13).
  Includes downstream CI workflow and local integration test script.
tags: [doctrine, amicron-platform, fb3, ci, v12.1]
priority: 1
---

# Spec: v12.1.0 Doctrine & amicron-platform Compatibility

## Issue
#324 (this spec) — index for #352-#358 (7 issues)

## Branch
`test/integration-conformance`

## Date
2026-07-07

## Status
COMPLETE — 8 issues closed (#324, #352-#358). doctrine-firebird-driver v3.14.0 + amicron-platform CI verified.

---

## Intent

Verify that php-firebird does not break the primary downstream consumer
(`satag/doctrine-firebird-driver`) or the primary customer application
(`amicron-platform`). Establish automated CI signal for the doctrine driver
and a local integration test script for the private amicron-platform.

This milestone is **customer-critical**: amicron-platform already upgraded
from php-firebird v7.3.0 to v12.0.0 (5 major versions, commit `2c39d461` on
`release/3.0.0-rc.2`). The v12.1.0 test suite must not introduce regressions
for this migration path.

---

## Background

### Production stack (from `release/3.0.0-rc.2` composer.lock)

| Component | Version | Source |
|-----------|---------|--------|
| PHP | ^8.4 | `composer.json` require |
| Symfony | 7.4.* | `composer.json` require |
| doctrine/doctrine-migrations-bundle | ^3.0 | `composer.json` require |
| satag/doctrine-firebird-driver | v3.14.0 (locked) | `composer.lock` |
| satag/amicron-entity-bundle | v6.1.0 | `composer.json` require |
| satwareag/php-firebird-stubs | ^12.0 | `composer.json` require (commit `16279426`) |
| PHPUnit | ^11.0 | `composer.json` require-dev |
| Firebird server | 3.0.13 | `amicron-firebird` Docker container |
| Firebird client | 5.0.x | System `libfbclient 5.0.4-1` |

### Migration history

- **v7.3.0 -> v12.0.0**: 5 major versions. Commit `2c39d461` ("fix(docker): upgrade
  php-firebird v7.3.0 to v12.0.0 in web and CI images")
- **Commit `80493c4b`**: "fix(docker): upgrade CI inline php-firebird to v12.0.0 +
  correct FB_API_VER comments" — customer already hit and fixed FB_API_VER issues
- **Commit `16279426`**: "feat(deps): upgrade driver to ^3.14.0, add php-firebird-stubs
  ^12.0, phpunit ^11.0" — stubs are now a downstream consumer

### Privacy boundary

amicron-platform is a private repository at `~/internal/amicron-platform` (internal
git server). The public php-firebird CI cannot clone it directly. Per the
IPADP L3 conformance privacy rules, no private repo URLs or references may appear
in public CI workflows.

**Decision**: amicron-platform compatibility is verified via a local script
(`scripts/test-amicron-platform.sh`, #353), not public CI. The doctrine-firebird-driver
(public, `~/external/doctrine-firebird-driver`) gets a full CI workflow (#352).

### Previous downstream issues

The v12.0.0-rc.11 hotfixes (5 issues #305-#309) were discovered through exactly
this kind of downstream integration testing with doctrine-firebird-driver.
Establishing permanent CI for this prevents regression.

---

## User Stories / Acceptance Goals

### Goal 1: doctrine-firebird-driver CI (GATE)

**Given** a PR to php-firebird that changes driver behavior
**When** the doctrine-downstream CI workflow runs
**Then** `doctrine-firebird-driver@v3.14.0` test suite passes green

**Success Criteria**:
- [ ] #352 `.github/workflows/doctrine-downstream.yml` created and passing on satware-main
- [ ] `v3.14.0` matrix target is required-green
- [ ] `main` matrix target runs as informational (continue-on-error)

### Goal 2: amicron-platform local validation

**Given** a developer with local access to `~/internal/amicron-platform`
**When** they run `scripts/test-amicron-platform.sh`
**Then** the script boots Symfony 7.4, validates Doctrine schema, runs PHPStan, runs PHPUnit

**Success Criteria**:
- [ ] #353 `scripts/test-amicron-platform.sh` created
- [ ] Script runs `doctrine:schema:validate`, PHPStan, PHPUnit against `amicron-firebird` container
- [ ] Script documented in AGENTS.md release procedure

### Goal 3: Amicron schema CRUD integration

**Given** the `amicron-demo.fdb` database (FB 3.0.13, real Amicron demo data)
**When** the integration test runs SELECT/INSERT/UPDATE/DELETE across all 5 major tables
**Then** all operations succeed, FK constraints are respected, no test rows leak

**Success Criteria**:
- [ ] #354 schema CRUD test passes on ADRESSEN/AUFTRAG/ARTIKEL/ATRPOS/KONTAKTE
- [ ] #355 BLOB column (ARTIKEL.BILD) roundtrip verified (105,183 bytes)
- [ ] #356 FK constraint cycle tested (AUFTRAG -> ADRESSEN, AUFTRAG -> AUFTRAG self-ref)
- [ ] All tests have --CLEAN-- sections

### Goal 4: Doctrine SchemaManager compatibility

**Given** Doctrine DBAL introspects `RDB$*` system tables via the driver
**When** `SchemaManager::listTables()` / `listTableColumns()` runs against `amicron-demo.fdb`
**Then** all 181 relations are listed, ADRESSEN columns match `fbird_field_info`

**Success Criteria**:
- [ ] #357 Symfony 7.4 + Doctrine Migrations boot test passes
- [ ] #358 SchemaManager list test passes (RED until #373 fbird_meta_data closes; use --SKIPIF--)

---

## Out of Scope

- Implementation of procedural gaps that Doctrine needs (#361 fbird_server_version, #373 fbird_meta_data) — those are M2/M9
- FB 4.0+ features (Doctrine driver doesn't use them yet)
- Performance benchmarking (separate concern)

---

## Constitution Alignment

| Article | Requirement | How Met |
|---------|-------------|---------|
| IPADP L3 | No private repo URLs in public CI | #353 is a local script only; #352 references only public doctrine-firebird-driver |
| II: Test-First | Tests before implementation | All integration tests written before M9 implementations |
| VII: Coverage Gate | Coverage >= 60% | Integration tests exercise real schema paths |

---

## Dependencies

- **Requires**: #322 (PDO conformance spec — Doctrine uses PDO driver)
- **Blocks**: v12.1.0 release (cannot ship if customer stack is broken)

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| #414 stubs BC break: expanding "Not yet implemented" block may break amicron-platform PHPStan | High | High | #353 script runs PHPStan as part of validation; CI catches via doctrine-downstream workflow |
| Doctrine SchemaManager fails on FB3 due to RDB$ table differences vs FB4+ | Low | Medium | #358 tests against FB 3.0.13 specifically |
| amicron-platform PHP 8.4 + PHPUnit 11 incompatibility with test suite | Low | Low | Already validated in release/3.0.0-rc.2 (customer ran it) |

---

## Issue Index

| Issue | Title | Type | Scope |
|-------|-------|------|-------|
| #352 | ci: doctrine-downstream.yml (v3.14.0 + main) | CI | S |
| #353 | ci: amicron-platform-downstream (local script) | CI | M |
| #354 | test: amicron-platform schema CRUD | Test | M |
| #355 | test: amicron-platform BLOB (ARTIKEL.BILD) | Test | S |
| #356 | test: amicron-platform FK constraint cycle | Test | S |
| #357 | test: Symfony 7.4 + Doctrine Migrations boot | Test | L |
| #358 | test: Doctrine SchemaManager listTables/Columns | Test (RED) | M |

# Spec: v12.1.0 - Integration Conformance Suite

**Status**: IN PROGRESS  
**Branch**: `test/integration-conformance`  
**Target release**: v12.1.0 (rc.1 by 2026-08-18, final by 2026-08-25)  
**Scope**: Tests + specs only. Implementation of gaps spawns separate `feat/*` branches after v12.1.0 ships the red/skip test suite.

## Intent

Raise php-firebird driver quality to match:
1. **Expectations of PHP devs** - full PDO Definition conformance + big-5 procedural parity
2. **Full Firebird client interface** - every IAttachment/ITransaction/IStatement/IResultSet/IBlob/IEvents/IService/IMessageMetadata/IMetadataBuilder/IUtil/IXpbBuilder method reachable

**Primary customer focus**: Firebird 3.0 full support (amicron-platform `release/3.0.0-rc.2`: Symfony 7.4 + PHP 8.4 + doctrine-firebird-driver v3.14.0 + FB 3.0.13).

## Background

v12.0.0 shipped 315 `.phpt` tests but audit revealed gaps:
- PDO Definition: 9 of 13 FETCH modes untested, `getColumnMeta` returns IM001 despite stubs claiming support, `FBIRD_TXN_*` isolation levels documented but untested
- Procedural: `fbird_fetch_array` (BOTH mode) regression vs legacy interbase, `fbird_ping` missing in procedural (exists in OO), per-connection error context is global single-slot
- Firebird client: every interface method reachable but no systematic coverage gate

## Living-spec concept

Each sub-spec below is an index of GitHub issues. As issues close, the spec stays current via the issue tracker. The GitHub issues are the public, searchable, +1-able surface; the spec files are the dev-facing summary.

## Milestones (with issue counts)

| Milestone | Issues | Due | Spec issue |
|---|---|---|---|
| M1: FB3 PDO Definition Conformance | 25 (#322, #328-#351) | 2026-07-21 | #322 |
| M2: FB3 Procedural API Parity | 27 (#323, #359-#384) | 2026-07-28 | #323 |
| M3: FB3 Doctrine & amicron-platform | 8 (#324, #352-#358) | 2026-08-04 | #324 |
| M4: FB3 Firebird Client Coverage | 20 (#325, #385-#403) | 2026-08-11 | #325 |
| M5: FB3 Cross-Version Compat | 7 (#326, #404-#409) | 2026-08-11 | #326 |
| M6: FB3 Documentation & Polish | 7 (#410-#416) | 2026-08-18 | (none, docs only) |
| M7: FB3 Test Infrastructure | 10 (#312-#321) | 2026-07-21 | (none, infra only) |
| **v12.1.0 total** | **104** | | |
| M8: FB4+/5+/6+ Coverage (stretch) | 20 (#327, #417-#435) | 2026-11-30 | #327 |
| M9: Implementation Work (backlog) | 6 (#436-#441) | no due date | (none) |

## Customer context (amicron-platform `release/3.0.0-rc.2`)

- **Stack**: Symfony 7.4.* + PHP ^8.4 + doctrine-migrations-bundle ^3.0 + `satag/doctrine-firebird-driver:^3.14.0` (locked v3.14.0)
- **Stubs consumer**: `satwareag/php-firebird-stubs:^12.0` (commit 16279426) - stubs changes must not break PHPStan
- **DB**: Firebird 3.0.13 via `amicron-firebird` Docker container (image pinned by SHA `a3bddf6e`)
- **Migration**: Already upgraded php-firebird v7.3.0 -> v12.0.0 (commit 2c39d461) - 5 major versions

## Constitution alignment

- **Article II Test-First**: every gap gets a RED test before implementation
- **Article III Memory Safety**: integration tests run under ASan/TSan
- **Article VII Coverage Gate**: bump 54% -> 60% (#318)

## Sub-specs (living indexes)

- [M1: PDO Definition Conformance](spec-v12.1-pdo-conformance.md) - issue #322
- [M2: Procedural API Parity](spec-v12.1-procedural-parity.md) - issue #323
- [M3: Doctrine & amicron-platform](spec-v12.1-doctrine-downstream.md) - issue #324
- [M4: Firebird Client Coverage](spec-v12.1-firebird-client-coverage.md) - issue #325
- [M5: Cross-Version Compat](spec-v12.1-cross-version-compat.md) - issue #326
- [M8 (stretch): FB4+/5+/6+ Coverage](spec-v13.0-fb4-plus-coverage.md) - issue #327

## Out of scope

- Implementation of any procedural gap (M2 feat issues) - these spawn separate `feat/*` branches in M9 after v12.1.0 ships the test suite
- FB 4.0+/5.0+/6.0 feature implementation (M8 stretch) - deferred to v13.0.0

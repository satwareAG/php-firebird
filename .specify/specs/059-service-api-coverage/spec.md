# Spec: Service API Test Coverage

**Issue**: #59
**Branch**: `test/service-api-coverage`
**Date**: 2026-03-02
**Status**: Approved

---

## Intent

Increase test coverage of the Firebird Service API from its current gaps (0% on
`fb_service.hpp`, ~72% on `fbird_service.c`) to ≥80% on both files.
This is a **test-only** change — no implementation code is modified.

---

## Background

Coverage analysis identified two critical gaps:

| File | Lines | Current Coverage | Target |
|------|-------|-----------------|--------|
| `src/cpp/fb_service.hpp` | 439 | 0% (0/174 measured) | ≥80% |
| `fbird_service.c` | 648 | ~72% | ≥80% |

The Service API provides backup/restore, user management, database maintenance, and
server information. The SIGSEGV fix (PR #70) added NULL guards — these error paths
need test coverage to prevent regressions.

### Public Functions in fbird_service.c

| Function | Purpose | Gap |
|----------|---------|-----|
| `fbird_add_user` | Add Firebird user | boundary values |
| `fbird_modify_user` | Modify user attributes | empty fields |
| `fbird_delete_user` | Delete user | non-existent user |
| `fbird_service_attach` | Connect to service manager | error paths |
| `fbird_service_detach` | Disconnect from service manager | NULL handle (PR#70 fix) |
| `fbird_backup` | Backup database | all FBIRD_BKP_* flags |
| `fbird_restore` | Restore database | all FBIRD_RES_* flags |
| `fbird_maintain_db` | Maintenance (sweep, shutdown) | all FBIRD_PRP_*/FBIRD_RPR_* |
| `fbird_db_info` | Database statistics | all info types |
| `fbird_server_info` | Server information | all info types |

### Static Helpers with Coverage Gaps

| Helper | Purpose | Gap |
|--------|---------|-----|
| `_php_fbird_free_service` | Destructor | NULL handle path (PR#70 fix) |
| `_php_fbird_user` | User create/modify/delete shared logic | field validation paths |
| `_php_fbird_backup_restore` | Shared backup/restore dispatch | option combinations |
| `_php_fbird_service_action` | Maintenance action dispatch | action type coverage |
| `_php_fbird_service_query` | Query response parsing | error/empty response paths |

---

## User Stories / Acceptance Goals

### Goal 1: Service API Functions Have ≥80% Test Coverage

**Given** the Docker test environment with a live Firebird server
**When** coverage report is generated with `scripts/coverage.sh`
**Then** `fbird_service.c` shows ≥80% line coverage in lcov

**Success Criteria**:
- [ ] `fbird_service.c` ≥80% line coverage (lcov)
- [ ] `fb_service.hpp` ≥80% line coverage (lcov)
- [ ] All 4 new `.phpt` test files pass in Docker
- [ ] Tests are skipped gracefully when Firebird server unavailable

### Goal 2: PR#70 Error Paths Are Explicitly Tested

**Given** the SIGSEGV fix — NULL handle guard in `_php_fbird_free_service`
**When** a backup/restore operation fails early (before `isc_service_start`)
**Then** the resource destructor runs without crashing

**Success Criteria**:
- [ ] Test exists that triggers the NULL-handle destructor path
- [ ] Test passes under ASan (no heap-use-after-free)

### Goal 3: All FBIRD_BKP_* / FBIRD_RES_* / FBIRD_PRP_* / FBIRD_RPR_* Constants Exercised

**Given** the Service API constants defined in `php_firebird.h`
**When** tests exercise all option combinations
**Then** each constant is referenced in at least one test

**Success Criteria**:
- [ ] All `FBIRD_BKP_*` constants used in `service_backup_restore.phpt`
- [ ] All `FBIRD_RES_*` constants used in `service_backup_restore.phpt`
- [ ] All `FBIRD_PRP_*` / `FBIRD_RPR_*` constants used in `service_maintenance_operations.phpt`

---

## Out of Scope

- No changes to `fbird_service.c` or `fb_service.hpp` implementation
- No new PHP-visible functions
- No Windows-only service paths (testing Linux Docker only)
- `fbird_server_info` constants — covered by existing `fbird_service_001.phpt`

---

## Test Files to Create

| File | Tests |
|------|-------|
| `tests/coverage/service_user_advanced.phpt` | username/password boundaries, duplicate add, non-existent delete, field validation |
| `tests/coverage/service_backup_restore.phpt` | All FBIRD_BKP_*/FBIRD_RES_* options, multi-file backup, portable format |
| `tests/coverage/service_maintenance_operations.phpt` | sweep, shutdown modes, set page buffers, repair options |
| `tests/coverage/service_error_paths.phpt` | NULL resource passed, attach to invalid host, destructor NULL-handle path |

---

## Constitution Alignment

| Article | Requirement | How Met |
|---------|-------------|---------|
| II: Test-First | Tests are the deliverable | These ARE the tests (no impl change) |
| III: Memory Safety | ASan/UBSan clean | error_paths test exercises PR#70 fix under ASan |
| VII: Coverage Gate | ≥80% on touched files | lcov validation required before merge |
| VIII: Docker Testing | Tests use `--SKIPIF--` | All 4 tests check Firebird connection |

---

## Dependencies

- **Requires**: PR #70 merged to `satware-main` ✅ (done)
- **Blocks**: #63 (final coverage validation)

---

## Open Questions

None — all details resolved.

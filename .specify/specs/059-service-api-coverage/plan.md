# Plan: Service API Test Coverage

**Issue**: #59
**Spec**: `.specify/specs/059-service-api-coverage/spec.md`
**Date**: 2026-03-02
**Status**: Approved

---

## Constitution Check

| Article | Gate | Status |
|---------|------|--------|
| I: C Extension First | No .c changes — test-only | ✅ N/A |
| II: Test-First | Tests ARE the deliverable | ✅ |
| III: Memory Safety | error_paths test validates NULL-guard fix under ASan | ✅ |
| VI: Atomic Commits | 1 test file per commit (4 commits) | ✅ |
| VII: Coverage Gate | lcov ≥80% verified before PR merge | ✅ |
| VIII: Docker Testing | All tests use `--SKIPIF--` with connection check | ✅ |

---

## Technical Approach

### Chosen Approach: Targeted Gap Coverage

Write 4 focused `.phpt` files, each targeting a specific coverage gap:

1. **User management edge cases** — exercise `_php_fbird_user()` branches not covered by
   `fbird_service_user.phpt` (boundary values, duplicate operations, missing fields)
2. **Backup/restore option flags** — exercise `_php_fbird_backup_restore()` with every
   `FBIRD_BKP_*` and `FBIRD_RES_*` constant combination
3. **Maintenance actions** — exercise `_php_fbird_service_action()` with all
   `FBIRD_PRP_*` and `FBIRD_RPR_*` option values
4. **Error paths** — exercise error branches in `fbird_service_attach`, `fbird_service_detach`,
   and the NULL-handle destructor path from PR #70

### Alternative: Extend Existing Tests

Rejected — existing test files are already committed with specific scope. Adding to them
would make them too large and hard to bisect failures.

---

## Files Affected

| File | Change Type | Description |
|------|-------------|-------------|
| `tests/coverage/service_user_advanced.phpt` | Create | User CRUD edge cases |
| `tests/coverage/service_backup_restore.phpt` | Create | All BKP/RES option flags |
| `tests/coverage/service_maintenance_operations.phpt` | Create | All PRP/RPR maintenance options |
| `tests/coverage/service_error_paths.phpt` | Create | Error branches, NULL handle path |

---

## Implementation Sequence

Each test file is one commit. Sequence chosen to validate core path first, error paths last.

1. `service_user_advanced.phpt` → `test(coverage): service user management edge cases (#59)`
2. `service_backup_restore.phpt` → `test(coverage): backup/restore option flag coverage (#59)`
3. `service_maintenance_operations.phpt` → `test(coverage): maintenance operation coverage (#59)`
4. `service_error_paths.phpt` → `test(coverage): service API error path coverage (#59)`

---

## Test Plan

### service_user_advanced.phpt

Covers `_php_fbird_user()` branches:

```text
- fbird_add_user with all optional fields (firstname, lastname, middle, passwd, uid, gid)
- fbird_add_user with minimal fields (only username + password)
- fbird_modify_user changing each optional field
- fbird_delete_user for non-existent user (expect warning/false, not crash)
- fbird_add_user with empty username (expect false)
- fbird_add_user duplicate (expect false/warning)
```

### service_backup_restore.phpt

Covers `_php_fbird_backup_restore()` option combinations:

```text
FBIRD_BKP_IGNORE_CHECKSUMS   — skip checksum validation
FBIRD_BKP_IGNORE_LIMBO       — skip limbo transactions
FBIRD_BKP_META_DATA_ONLY     — structure only, no data
FBIRD_BKP_NO_GARBAGE_COLLECT — skip garbage collection
FBIRD_BKP_OLD_DESCRIPTIONS   — use legacy format
FBIRD_BKP_NON_TRANSPORTABLE  — non-portable backup
FBIRD_BKP_CONVERT            — convert external tables

FBIRD_RES_DEACTIVATE_IDX     — restore without activating indices
FBIRD_RES_NO_SHADOW          — don't restore shadows
FBIRD_RES_NO_VALIDITY        — bypass validity checks
FBIRD_RES_ONE_AT_A_TIME      — commit after each table
FBIRD_RES_REPLACE            — overwrite existing DB
FBIRD_RES_CREATE             — create new DB
FBIRD_RES_USE_ALL_SPACE      — fill all pages
```

### service_maintenance_operations.phpt

Covers `_php_fbird_service_action()` with maintenance options:

```text
FBIRD_PRP_PAGE_BUFFERS       — set page buffer count
FBIRD_PRP_SWEEP_INTERVAL     — set sweep interval
FBIRD_PRP_SHUTDOWN_DB        — shutdown database (+ modes)
FBIRD_PRP_ONLINE             — bring database online
FBIRD_PRP_RES_ONLINE_MODE    — online mode options
FBIRD_PRP_SET_SQL_DIALECT    — set SQL dialect
FBIRD_PRP_RESERVE_SPACE      — reserve page space
FBIRD_PRP_WRITE_MODE_ASYNC   — async write mode
FBIRD_PRP_WRITE_MODE_SYNC    — sync write mode
FBIRD_PRP_ACCESS_MODE_READONLY  — read-only mode
FBIRD_PRP_ACCESS_MODE_READWRITE — read-write mode

FBIRD_RPR_CHECK_DB           — validate database
FBIRD_RPR_IGNORE_CHECKSUM    — ignore bad checksums
FBIRD_RPR_KILL_SHADOWS       — remove shadow files
FBIRD_RPR_MEND_DB            — repair database
FBIRD_RPR_VALIDATE_DB        — validate pages
FBIRD_RPR_FULL               — full validation
FBIRD_RPR_SWEEP_DB           — sweep (in repair)
```

### service_error_paths.phpt

Covers error branches and the PR #70 NULL-guard fix:

```text
- fbird_service_attach with invalid host → expect false + warning
- fbird_service_attach with wrong credentials → expect false + warning
- fbird_backup with invalid service handle (use NULL / false) → expect false
- fbird_restore with invalid service handle → expect false
- Trigger destructor NULL-handle path: call fbird_service_detach on
  a handle that was already detached (or manually corrupt the handle)
- fbird_maintain_db with invalid service → expect false
```

---

## Sanitizer Validation

```bash
# After each test file committed:
docker compose run --rm php83-dev /ext/scripts/run-sanitizer.sh
# service_error_paths.phpt specifically exercises the ASan-sensitive NULL guard
```

---

## Coverage Validation

```bash
docker compose run --rm php83-dev /ext/scripts/coverage.sh
# Check in coverage/lcov.info or HTML report:
#   fbird_service.c: ≥80% lines
#   fb_service.hpp:  ≥80% lines
```

---

## Risks

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Some FBIRD_PRP_* ops need specific DB state (e.g., shutdown) | High | Medium | Use separate test DB, restore online after shutdown |
| Multi-file backup requires filesystem setup | Medium | Low | Skip multi-file if tmpfs not available in Docker |
| fb_service.hpp coverage depends on which code paths are linked | Medium | High | Verify with nm/objdump that C++ methods are actually called |

---

## Definition of Done

- [ ] 4 new `.phpt` files created in `tests/coverage/`
- [ ] All 4 tests pass in Docker (`make test`)
- [ ] `fbird_service.c` coverage ≥80% (lcov)
- [ ] `fb_service.hpp` coverage ≥80% (lcov)
- [ ] ASan + UBSan: no errors from `service_error_paths.phpt`
- [ ] Valgrind: no definite leaks from new tests
- [ ] PR #71 (or next) approved and merged to `satware-main`
- [ ] Issue #59 closed

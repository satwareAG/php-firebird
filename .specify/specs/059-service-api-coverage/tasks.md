# Tasks: Service API Test Coverage

**Issue**: #59
**Plan**: `.specify/specs/059-service-api-coverage/plan.md`
**Date**: 2026-03-02

---

## Phase 1: Setup

- [x] [T01] [P0] Merge PR #70 to `satware-main` ✅ (done 2026-03-02)
- [ ] [T02] [P0] Create branch `test/service-api-coverage` from `satware-main`
- [ ] [T03] [P0] Verify Docker environment: `docker compose run --rm php83-dev make test`

---

## Phase 2: Test Files (one commit each)

- [ ] [T10] [P0] Create `tests/coverage/service_user_advanced.phpt`
  - fbird_add_user with all optional fields (firstname, lastname, middle, uid, gid)
  - fbird_add_user with minimal fields (username + password only)
  - fbird_modify_user changing each optional field independently
  - fbird_delete_user for non-existent user (warns but no crash)
  - fbird_add_user with duplicate username (expects false)
  - Commit: `test(coverage): service user management edge cases (#59)`

- [ ] [T11] [P0] Create `tests/coverage/service_backup_restore.phpt`
  - fbird_backup with each FBIRD_BKP_* flag separately
  - fbird_backup with combined flags (OR'd)
  - fbird_restore with each FBIRD_RES_* flag separately
  - fbird_restore to new location (FBIRD_RES_CREATE)
  - fbird_restore overwrite (FBIRD_RES_REPLACE)
  - Commit: `test(coverage): backup/restore option flag coverage (#59)`

- [ ] [T12] [P0] Create `tests/coverage/service_maintenance_operations.phpt`
  - fbird_maintain_db with FBIRD_RPR_CHECK_DB
  - fbird_maintain_db with FBIRD_RPR_VALIDATE_DB + FBIRD_RPR_FULL
  - fbird_maintain_db with FBIRD_RPR_SWEEP_DB
  - fbird_maintain_db with FBIRD_PRP_SWEEP_INTERVAL
  - fbird_maintain_db with FBIRD_PRP_PAGE_BUFFERS
  - fbird_maintain_db with FBIRD_PRP_WRITE_MODE_ASYNC + FBIRD_PRP_WRITE_MODE_SYNC
  - fbird_maintain_db with FBIRD_PRP_ACCESS_MODE_READONLY + FBIRD_PRP_ACCESS_MODE_READWRITE
  - Commit: `test(coverage): maintenance operation coverage (#59)`

- [ ] [T13] [P0] Create `tests/coverage/service_error_paths.phpt`
  - fbird_service_attach to invalid host → expect false, error set
  - fbird_service_attach with wrong password → expect false
  - fbird_backup with false/invalid service resource → expect false
  - fbird_restore with false/invalid service resource → expect false
  - fbird_maintain_db with false/invalid service → expect false
  - Double-detach: attach, detach, detach again → no crash (NULL guard)
  - Commit: `test(coverage): service API error path coverage (#59)`

---

## Phase 3: Validation

- [ ] [T20] [P0] Run full test suite in Docker
  ```bash
  docker compose run --rm php83-dev make test
  ```
  All tests must pass (new tests skip if no Firebird server, must not FAIL)

- [ ] [T21] [P0] Generate coverage report
  ```bash
  docker compose run --rm php83-dev /ext/scripts/coverage.sh
  ```
  Check `tests/coverage/` HTML report for:
  - `fbird_service.c` ≥80% line coverage
  - `fb_service.hpp` ≥80% line coverage

- [ ] [T22] [P0] Run sanitizers
  ```bash
  docker compose run --rm php83-dev /ext/scripts/run-sanitizer.sh
  ```
  Zero ASan/UBSan errors on all new test files

- [ ] [T23] [P1] Run Valgrind
  ```bash
  docker compose run --rm php83-dev /ext/scripts/run-valgrind.sh
  ```
  No definite leaks in new test execution paths

- [ ] [T24] [P1] If coverage <80% after 4 files, identify remaining gaps and add targeted tests

---

## Phase 4: PR & Merge

- [ ] [T30] [P0] Push branch: `git push -u origin test/service-api-coverage`
- [ ] [T31] [P0] Open PR: `gh pr create --base satware-main --title "test(coverage): Service API comprehensive coverage (#59)"`
- [ ] [T32] [P0] Verify all 26 CI checks pass
- [ ] [T33] [P0] Squash merge: `gh pr merge --squash --delete-branch`
- [ ] [T34] [P0] Close issue #59: `gh issue close 59 --comment "Coverage ≥80% achieved for fbird_service.c and fb_service.hpp"`

---

## Progress Tracking

| Phase | Tasks | Done | Status |
|-------|-------|------|--------|
| Setup | 3 | 1 | 🔄 |
| Test Files | 4 | 0 | ⬜ |
| Validation | 5 | 0 | ⬜ |
| PR & Merge | 5 | 0 | ⬜ |
| **Total** | **17** | **1** | 🔄 |

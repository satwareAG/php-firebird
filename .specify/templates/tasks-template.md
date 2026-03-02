# Tasks: {Feature Name}

**Issue**: #{issue-number}
**Plan**: `.specify/specs/{dir}/plan.md`
**Date**: {YYYY-MM-DD}

> Format: `- [ ] [TaskID] [Priority] Description → file-path`
> Priority: P0 (blocking), P1 (required), P2 (nice-to-have)
> Parallel tasks: mark with [P] — can run concurrently

---

## Phase 1: Setup & Prerequisites

- [ ] [T01] [P0] Create branch `{branch-name}` from `satware-main`
- [ ] [T02] [P0] Verify Docker environment works: `docker compose run --rm php83-dev make test`

---

## Phase 2: Tests (Write BEFORE implementation — RED phase)

- [ ] [T10] [P0] Create test file → `tests/coverage/{name_1}.phpt`
- [ ] [T11] [P0] Create test file → `tests/coverage/{name_2}.phpt`
- [ ] [T12] [P0] Verify tests FAIL without implementation changes (confirm RED)

---

## Phase 3: Implementation

- [ ] [T20] [P0] {Implementation step 1} → `{file.c}`
- [ ] [T21] [P0] {Implementation step 2} → `{file.c}`
- [ ] [T22] [P1] {Implementation step 3} → `{file.c}`

---

## Phase 4: Validation

- [ ] [T30] [P0] Run tests in Docker → all pass (GREEN)
- [ ] [T31] [P0] Run coverage → `docker compose run --rm php83-dev /ext/scripts/coverage.sh`
- [ ] [T32] [P0] Verify coverage ≥80% for touched files (check lcov report)
- [ ] [T33] [P0] Run sanitizers → `docker compose run --rm php83-dev /ext/scripts/run-sanitizer.sh`
- [ ] [T34] [P1] Run Valgrind → `docker compose run --rm php83-dev /ext/scripts/run-valgrind.sh`

---

## Phase 5: PR & Merge

- [ ] [T40] [P0] Push branch, open PR with issue reference
- [ ] [T41] [P0] Verify all 26 CI checks pass
- [ ] [T42] [P0] Merge PR → close issue #{issue-number}

---

## Progress Tracking

| Phase | Tasks | Done | Status |
|-------|-------|------|--------|
| Setup | 2 | 0 | ⬜ |
| Tests | 3 | 0 | ⬜ |
| Implementation | 3 | 0 | ⬜ |
| Validation | 5 | 0 | ⬜ |
| PR & Merge | 3 | 0 | ⬜ |
| **Total** | **16** | **0** | ⬜ |

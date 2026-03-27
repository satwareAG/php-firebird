# Implementation Plan

[Overview]
Consolidate all v10.0.0 development branches into a single `dev/v10` branch, run the full test suite (report-only), and update the SDD plan.

This merges 4 branches containing independent and dependent changes on top of the v9.0.0 `satware-main` baseline. The goal is a unified `dev/v10` branch with all work integrated, a test report documenting current pass/fail state (no fixes), and updated plan documents reflecting consolidation status.

[Types]
No type system changes. This is a branch merge and test reporting task.

N/A

[Files]
No source code file modifications. Only git operations and documentation updates.

- `docs/plans/v10-branch-consolidation-plan.md` - This plan document (new)
- `docs/plans/spec-implementation-plan.md` - Update with consolidation status
- `docs/ROADMAP-v10.0.0.md` - Update with consolidation status
- `VERSION` - Will be `10.0.0-dev` after merge (already set on modernization branch)

[Functions]
No function changes. This is a git merge operation.

N/A

[Classes]
No class changes. This is a git merge operation.

N/A

[Dependencies]
No dependency changes. Docker environment used for testing remains unchanged.

N/A

[Testing]
Run full `.phpt` test suite via Docker (report-only, no fixes).

Test execution: `docker compose run --rm php83-dev /ext/scripts/test.sh`
- 222 `.phpt` test files in `tests/`
- Baseline: 247/247 passing on `satware-main`
- Expected: Some failures due to v10 modernization changes (void* elimination, utils rewrite)
- Output: Capture full test report to `docs/plans/v10-test-report.txt`
- No test fixes to be applied

[Implementation Order]
Sequential merge following dependency chain, then test and document.

1. **Create `dev/v10` from `satware-main`**: `git checkout -b dev/v10 satware-main` - clean baseline at v9.0.0 (247/247 tests passing)
2. **Merge `v10.0.0-modernization`**: `git merge v10.0.0-modernization` - VERSION bump to 10.0.0-dev only, should be clean
3. **Merge `fix/v10-p0-legacy-handle-events`**: `git merge fix/v10-p0-legacy-handle-events` - extends modernization with build fixes, utils v2 rewrite, migration docs; should be clean (child branch)
4. **Merge `origin/feat/fix-transaction-class-conflict`**: `git merge origin/feat/fix-transaction-class-conflict` - independent branch (Transaction to TransactionManager rename); LOW conflict risk
5. **Merge `origin/fix/ci-composer-install`**: `git merge origin/fix/ci-composer-install` - independent branch (CI composer download fixes); MEDIUM conflict risk (branches from older commit)
6. **Run full test suite**: `docker compose run --rm php83-dev /ext/scripts/test.sh 2>&1 | tee docs/plans/v10-test-report.txt` - capture results
7. **Update plan documents**: Update `docs/plans/spec-implementation-plan.md` and `docs/ROADMAP-v10.0.0.md` with consolidation status and test results summary

## Conflict Resolution Strategy

- For merge conflicts in CI files (step 5): accept incoming changes (CI fixes are the purpose)
- For merge conflicts in C source files: accept incoming changes (feature branches have the newer code)
- For merge conflicts in VERSION: keep `10.0.0-dev`
- Use `git merge --no-edit` to avoid editor prompts; if conflicts arise, resolve manually then `git add . && git commit --no-edit`
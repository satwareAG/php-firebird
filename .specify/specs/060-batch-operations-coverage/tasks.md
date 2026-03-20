# Tasks: Batch Operations Test Coverage

**Issue**: #60
**Branch**: `test/batch-operations-coverage`
**Updated**: 2026-03-02

---

## Status

- [x] T1: Create `tests/coverage/batch_error_paths.phpt`
- [x] T2: Create `tests/coverage/batch_advanced_types.phpt`
- [x] T3: Create `tests/coverage/batch_limits.phpt`
- [x] T4: Create `plan.md` and `tasks.md`
- [x] T5: Commit + push + open PR against `satware-main` ✅ (PR #73 merged)
- [x] T6: CI passes (Docker Firebird 4.0+) ✅ (done 2026-03-02)
- [x] T7: Coverage report shows `firebird_utils.cpp` ≥80% and batch paths in `firebird.c` ≥80% ✅ (done 2026-03-02)

---

## Task Detail

### T1 — batch_error_paths.phpt

Tests all four batch functions with invalid (false) handles, wrong resource type
(connection instead of prepared stmt), and cancel-then-execute lifecycle.

### T2 — batch_advanced_types.phpt

Tests DATE/TIME/TIMESTAMP/DECIMAL type binding in batch, NULL for nullable columns,
NULL for NOT NULL column (should fail), and verifies roundtrip via SELECT.

### T3 — batch_limits.phpt

Tests create+immediate-cancel (no inserts), two sequential batch executes on the
same prepared statement, large batch (100 rows), and cancel-mid-batch with
rollback (verifying rows are not committed).

### T5 — PR

```bash
git add tests/coverage/batch_*.phpt .specify/specs/060-batch-operations-coverage/
git commit -m "test(coverage): batch operations coverage (#60)"
git push -u origin test/batch-operations-coverage
gh pr create --base satware-main --title "test(coverage): Batch API comprehensive coverage (#60)" \
  --body "..."
```

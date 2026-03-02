# Plan: Batch Operations Test Coverage

**Issue**: #60
**Branch**: `test/batch-operations-coverage`
**Date**: 2026-03-02

---

## Architecture

### Files Modified/Created

| File | Action | Rationale |
|------|--------|-----------|
| `tests/coverage/batch_error_paths.phpt` | **Create** | Cover NULL handle guards, wrong resource type, cancel-after-cancel |
| `tests/coverage/batch_advanced_types.phpt` | **Create** | Cover DATE/TIME/TIMESTAMP/DECIMAL and NULL column values |
| `tests/coverage/batch_limits.phpt` | **Create** | Cover multiple sequential batches, large batch (100 rows), cancel mid-batch |

### Coverage Strategy

The existing `fbird_batch_*.phpt` tests cover:
- Basic add/execute flow (`fbird_batch_001.phpt`)
- Blob batch operations (`fbird_batch_blob_001.phpt`)
- Detailed error reporting with duplicates (`fbird_batch_errors_001.phpt`)
- Multitype inserts (`fbird_batch_multitype_001.phpt`)
- OO wrapper (`fbird_batch_oo_001.phpt`)

**Gap analysis** — new tests add:
1. **Error paths** — invalid resource handles passed to each batch function
2. **Type coverage** — DATE/TIME/TIMESTAMP/DECIMAL columns, NULL values in nullable columns
3. **Limit & lifecycle** — create-cancel without execute, sequential re-use, large batches

### FB_API_VER Guard

All three test files use the same skip pattern:
```php
if (!function_exists('fbird_batch_create')) die('skip ...');
if (get_fb_version() < 4.0) die('skip ...');
```

### C++ Coverage Path

`firebird_utils.cpp` is exercised when any batch operation succeeds against a
live Firebird 4.0+ instance. The `batch_advanced_types.phpt` and
`batch_limits.phpt` tests drive the `IBatch::execute()` and
`IBatch::close()` C++ paths which are currently at <30% coverage.

---

## Risk

| Risk | Mitigation |
|------|-----------|
| `fbird_batch_cancel` semantics vary (returns true/non-false) | Used `$r === true \|\| $r !== false` |
| Batch re-use after execute may not reset message buffer | Created fresh batch per test segment |
| `--EXPECTF--` output may differ on FB error wording | Used `bool(true)` assertions, not error text |

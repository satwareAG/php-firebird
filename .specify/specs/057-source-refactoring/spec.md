# Spec: Source Refactoring & API Cleanup

**Issue**: #57
**Branch**: `refactor/split-firebird-c`
**Date**: 2026-03-02
**Status**: Draft — Implement AFTER all coverage issues (#58–#63) are merged

---

## Intent

Split `firebird.c` (3668 lines) and simplify `fbird_query_exec.c` (1681 lines) before
v7.0.0-final. Enables downstream projects (doctrine-firebird-driver) to build against
a maintainable codebase. **No functional changes to any `fbird_*` public API.**

---

## Background

| File | Lines Now | Target |
|------|-----------|--------|
| `firebird.c` | 3668 | <1500 (core only) |
| `fbird_query_exec.c` | 1681 | <800 |

`firebird.c` currently contains 6 distinct logical modules mixed together:
- MINIT/MSHUTDOWN/MINFO + INI + globals (core)
- Error handling (errmsg, errcode, sqlstate, escape_string, exception_mode)
- Connection management (connect, pconnect, close, drop_db, connection_info)
- Transaction management (trans, commit, rollback, savepoint, trans_info)
- Batch operations (batch_create, batch_add, batch_execute, batch_cancel, batch_blob)
- Generator (gen_id)

---

## Extraction Plan (Phase 1)

| New File | Functions Extracted | Est. Lines |
|----------|---------------------|------------|
| `fbird_error.c` | `fbird_errmsg`, `fbird_errcode`, `fbird_sqlstate`, `fbird_escape_string`, `fbird_set_event_handler`, `fbird_free_event_handler`, exception_mode helpers | ~200 |
| `fbird_connection.c` | `fbird_connect`, `fbird_pconnect`, `fbird_close`, `fbird_drop_db`, `fbird_connection_info`, connection pool helpers | ~600 |
| `fbird_transaction.c` | `fbird_trans`, `fbird_commit`, `fbird_rollback`, `fbird_commit_ret`, `fbird_rollback_ret`, `fbird_add_def_trans` | ~600 |
| `fbird_batch.c` | `fbird_batch_create`, `fbird_batch_add`, `fbird_batch_execute`, `fbird_batch_cancel`, `fbird_batch_blob_create`, `fbird_batch_blob_add` | ~500 |
| Core `firebird.c` | MINIT, MSHUTDOWN, MINFO, INI, globals, `fbird_gen_id` | ~800 |

---

## Phase 2: fbird_query_exec.c Simplification

1. Evaluate overlap: `fbird_execute_statement`, `fbird_execute_query`, `fbird_execute_auto`
2. Extract large sub-functions from `_php_fbird_exec` (1000+ lines):
   - Parameter binding dispatch → `fbird_query_bind.c`
   - Result handling dispatch → `fbird_result.c`
   - Keep execution core in `fbird_query_exec.c`
3. Target: `fbird_query_exec.c` < 800 lines, no function > 200 lines

---

## Phase 3: Dead Code Removal

- Identify unused `static` functions via `--Wunused-function`
- Remove over-engineered error path abstractions
- Simplify double-dispatch patterns

---

## Acceptance Criteria

- [ ] All source files < 1500 lines
- [ ] No function > 200 lines  
- [ ] All existing tests pass (135+/138) — zero regressions
- [ ] No API breaking changes (fbird_* signature preserved)
- [ ] Memory sanitizer tests pass on all split files
- [ ] `config.m4` + `config.w32` updated with new file names
- [ ] CI matrix green (PHP 8.1–8.5 × Firebird 3.0–5.0)

---

## Out of Scope

- Functional changes to any `fbird_*` public API
- Changing parameter signatures
- OO wrapper (`src/Firebird/`) changes
- Windows-only code changes (but config.w32 MUST be updated)

---

## Constitution Alignment

| Article | Requirement | How Met |
|---------|-------------|---------|
| I: C Extension First | All splits are C files | ✅ |
| II: Test-First | Existing 135+ tests are the safety net | Must all pass |
| III: Memory Safety | Each split verified with ASan/UBSan | Per-split CI run |
| VI: Atomic Commits | One module extraction per commit | Enforced by commit plan |
| VII: Coverage Gate | Coverage must not drop after refactor | Run lcov before+after |
| IX: SDD | Complex change >3 files — spec required | This document |

---

## Dependencies

- **Requires**: #58, #59, #60, #61, #62, #63 all merged (coverage ≥80% first)
- **Blocks**: #66 (rc.50 release should ideally include final refactoring)

---

## Open Questions

- [ ] Does `fbird_batch.c` extraction require `#if FB_API_VER >= 40` guards throughout?
  — Recommended: Yes, keep existing guards and replicate them in new file header
- [ ] Should `fbird_gen_id` stay in `firebird.c` or move to a `fbird_sequence.c`?
  — Recommended: Stay in core `firebird.c` (small, tightly coupled to MINIT globals)

> Plan and tasks to be created when coverage issues are all merged and this issue becomes active.

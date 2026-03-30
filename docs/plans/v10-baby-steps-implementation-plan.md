---
# v10 Release — Baby Steps Implementation Plan

**Created:** 2026-03-30
**Author:** Jane Alesi / Cline
**Status:** COMPLETED 2026-03-30 — All phases executed and v10.0.1 released.

---

## Objective

Make v10.0.0 (and patch v10.0.1) the zero-open-item definitive release that becomes the basis for all satware AG products using Firebird. Primary optimization target: **PHP 8.4 + Firebird 3 (latest)**.

## Project State at Start

| Category | Count | Detail |
|----------|-------|--------|
| Open issues | 1 | #135 RAM leak in fbird_query() DML |
| Open milestones | 2 | v10.0.0 (empty), v11.0.0 (stale features already in v10) |
| Stale specs | 5 | All shipped but spec-132 missing RELEASED header |
| Stale plans | 8 | Multiple completion markers missing |
| project-brief.md | outdated | Referenced v7.2.0 |

---

## Phase 1: Fix Issue #135 — Server-side RAM Leak

### Root Cause Analysis

`StatementWrapper::free()` in `src/cpp/fb_statement.hpp` called `IStatement::release()` instead of `IStatement::free()`. The Firebird OO API `release()` only decrements the C++ interface refcount without telling the server to free the prepared statement handle. With 1000+ fbird_query() DML calls, each accumulated a server-side prepared statement until request end (~1.5 GB reported).

Same bug in `closeCursor()`: `IResultSet::release()` instead of `IResultSet::close()`.

### Steps

- [x] **1.1** Research upstream fix (FirebirdSQL/php-firebird#101)
- [x] **1.2** Confirm bug in `src/cpp/fb_statement.hpp` — `closeCursor()` and `free()` both use `release()` instead of proper OO API methods
- [x] **1.3** Fix `closeCursor()`: use `fb_get_master_interface()` + `CheckStatusWrapper` + `IResultSet::close()`
- [x] **1.4** Fix `free()`: same pattern using `IStatement::free()` (DSQL_drop equivalent)
- [x] **1.5** Write `tests/fbird_query_stmt_release_001.phpt` — 500 INSERTs + 100 UPDATEs, verify row counts and connection stability
- [x] **1.6** Build clean on `php84-fb3-dev` container — PASS
- [x] **1.7** Test PASS on PHP 8.4 + Firebird 3
- [x] **1.8** Comment on GitHub issue #135 with fix details, assign to v10.0.1 milestone
- [x] **1.9** Close issue #135

---

## Phase 2: GitHub Milestone Cleanup

- [x] **2.1** Create v10.0.1 milestone (#9) for the RAM leak fix
- [x] **2.2** Assign #135 to v10.0.1, close it
- [x] **2.3** Close v10.0.1 milestone (#9) after #135 closed
- [x] **2.4** Close v11.0.0 milestone (#8) — all features shipped in v10.0.0

---

## Phase 3: Spec Archive (mark all shipped specs RELEASED)

- [x] **3.1** `specs/spec-120-typed-connection.md` — already had RELEASED header
- [x] **3.2** `specs/spec-132-legacy-wrappers.md` — added RELEASED v10.0.0 header
- [x] **3.3** `specs/spec-v10-features.md` — already had RELEASED header
- [x] **3.4** `specs/spec-v10-legacy-removal.md` — already had RELEASED header
- [x] **3.5** `specs/spec-v10-void-star-elimination.md` — already had RELEASED header

---

## Phase 4: Plans & Roadmaps — Mark COMPLETED

- [x] **4.1** `docs/plans/implementation_plan.md` — COMPLETED 2026-03-30
- [x] **4.2** `docs/plans/spec-implementation-plan.md` — COMPLETED 2026-03-30
- [x] **4.3** `docs/plans/v10-branch-consolidation-plan.md` — COMPLETED 2026-03-30
- [x] **4.4** `docs/ROADMAP-2026-03-23.md` — COMPLETED 2026-03-30
- [x] **4.5** `docs/ROADMAP-v9.0.0.md` — COMPLETED 2026-03-23
- [x] **4.6** `docs/plans/v7.4.0-roadmap.md` — COMPLETED (features merged into v10 baseline)
- [x] **4.7** `docs/plans/2026-03-05-issues-milestones-plan.md` — COMPLETED 2026-03-30
- [x] **4.8** `docs/plans/RELEASE_STRATEGY.md` — COMPLETED (operational for v10.0.1)

---

## Phase 5: Documentation Updates

- [x] **5.1** `docs/product/project-brief.md`: Version 7.2.0 → 10.0.0, Last Updated 2026-03-30
- [x] **5.2** `stubs/composer.json`: already `"10.0.x-dev"` branch-alias — no change needed
- [x] **5.3** `README.md`: already reflects v10.0.0 — no change needed

---

## Phase 6: Patch Release v10.0.1

- [x] **6.1** Update `VERSION` → `10.0.1`
- [x] **6.2** Add `## [10.0.1] - 2026-03-30` section to `CHANGELOG.md`
- [x] **6.3** Update CHANGELOG links (`[Unreleased]` and `[10.0.1]` diff links)
- [x] **6.4** Commit all changes on `docs/v10-post-release-status` branch
- [x] **6.5** Push branch, create PR #136, squash-merge to `satware-main`
- [x] **6.6** Tag `v10.0.1`, push tag
- [x] **6.7** Create GitHub release at https://github.com/satwareAG/php-firebird/releases/tag/v10.0.1

---

## Final State Verification

| Check | Result |
|-------|--------|
| Open GitHub issues | **0** |
| Open GitHub milestones | **0** |
| VERSION file | **10.0.1** |
| Latest tag | **v10.0.1** |
| CHANGELOG top entry | **[10.0.1] - 2026-03-30** |
| PHP 8.4 + FB3 test | **PASS** (600+ DML statements) |

---

## Key Technical Detail

The fix in `src/cpp/fb_statement.hpp` (lines 430–510):

```cpp
// BEFORE (leak): only decrements C++ refcount, no server-side free
statement_->release();

// AFTER (fixed): explicitly frees server-side prepared statement
Firebird::IMaster* master = Firebird::fb_get_master_interface();
Firebird::IStatus* fb_status = master->getStatus();
fb_status->init();
Firebird::CheckStatusWrapper cs(fb_status);
statement_->free(&cs);  // DSQL_drop equivalent
fb_status->dispose();
```

The `IStatement::free()` and `IResultSet::close()` methods both:
1. Free/close the server-side resource (prepared statement / cursor)
2. Handle refcounting internally (no separate `release()` needed)

This pattern matches how `firebird_utils.cpp` handles statements in the batch API (lines 1955+).

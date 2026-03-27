# Implementation Plan: v10.0.0 Clean Release Gate

[Overview]
Retract the premature v10.0.0 tag, correct all fabricated changelog content, close/defer open issues honestly, run the full 12-container test matrix, and validate with Valgrind and ASAN before a clean merge to satware-main.

The current v10.0.0 tag and branch are corrupted: the CHANGELOG claims features that do not exist (fbird_set_shutdown_active, Firebird\TBuilder, savepoints, limbo transactions, etc.) while the actual release is the v9 codebase with linker fixes and PDO integration. The v10.0.0 milestone has 0 closed issues and 1 open (#120). The working directory is dirty with uncommitted spec changes and 16 deleted .loT build artifacts. All of this must be corrected before satware-main merge.

[Types]
No new types required - this is an audit/cleanup plan with no new C or PHP types.

The only type-related change is removing phantom function signatures from CHANGELOG.md that were never implemented. All C structs, PHP classes, and type mappings in the actual codebase remain unchanged from the v9 restore baseline.

[Files]
This plan requires changes to documentation, git state, and CI artefacts.

New files to create:
- None

Existing files to modify:
- `CHANGELOG.md` - delete fabricated [10.0.0] content, reorder so [10.0.0] appears before [9.0.0], list only actually-shipped changes
- `specs/spec-v10-features.md` - update success criteria to reflect true state (unchecked = deferred to v11)
- `specs/spec-v10-legacy-removal.md` - update success criteria to reflect true state
- `specs/spec-132-legacy-wrappers.md` - update to mark as deferred
- `docs/ROADMAP-v10.0.0.md` - update to reflect actual v10 scope

Files to delete or gitignore:
- `*.loT` files - PHP build artifacts, should be in .gitignore
- `NEXT_STEPS.md` - stale plan for a dead branch (fix/v10-p0-legacy-handle-events), commit deletion
- `docs/plans/v10-test-report.txt` - untracked result file, add to .gitignore or commit
- `implementation_plan.md` - this file itself, commit after task completes

Git state to change:
- Delete tag `v10.0.0` locally: `git tag -d v10.0.0`
- Delete tag `v10.0.0` on GitHub: `git push origin :refs/tags/v10.0.0`
- Commit all pending changes in a `chore: v10.0.0 release audit` commit
- Re-create tag on clean commit

GitHub to change:
- Issue #132: close with comment "legacy_wrappers.c excluded from build by design; direct isc_* calls retained"
- Issue #120: update milestone to v11.0.0 (or create v11 milestone)
- v10.0.0 milestone: update description to reflect actual shipped scope

[Functions]
No function changes required.

All PHP_FUNCTION bodies in the extension are correct (v9 restore is the source of truth). The CHANGELOG will be corrected to remove references to functions that do not exist: fbird_set_shutdown_active, fbird_set_shutdown_active_oo, fbird_get_limbo_transactions, fbird_reconnect_transaction, fbird_savepoint, fbird_rollback_savepoint, fbird_release_savepoint, fbird_batch_add_blob_stream, fbird_batch_append_blob_data, fbird_batch_set_default_bpb.

Note: Some of these (fbird_batch_add_blob_stream, fbird_batch_append_blob_data, fbird_batch_set_default_bpb) may actually exist as test files. Verify with grep before removing from changelog.

[Classes]
No class changes required.

Firebird\TBuilder and Firebird\BlobStream mentioned in CHANGELOG do NOT exist - remove from changelog. All actual OOP classes (Firebird\Connection, Firebird\Transaction, Firebird\ResultSet, Firebird\Statement, Firebird\Blob, Firebird\Service) remain as shipped.

[Dependencies]
No dependency changes.

Stubs are already 89/89 in sync per the previous session. No stubs modifications needed since we are not adding or removing PHP functions.

[Testing]
The full test gate requires five sequential phases.

Phase 1: Stubs sync check
- Command: `bash scripts/check-stubs-sync.sh` (must exit 0, 89/89 match)

Phase 2: Core test (already passing, re-confirm)
- Container: `php83-dev` vs `firebird40`
- Command: `docker compose exec php83-dev bash -c "/ext/scripts/build.sh && /ext/scripts/test.sh"`
- Minimum threshold: 254/262 pass, 0 fail

Phase 3: Full 12-container matrix
- Command: `cd docker && bash ../scripts/test_matrix.sh`
- Covers: php82/83/84/85 x fb3/fb4/fb5 (12 combinations)
- Each must build and pass; note any regressions per-container

Phase 4: ASAN memory check
- Container: `php83-asan` (Dockerfile-asan)
- Command: `./scripts/run-sanitizer.sh --mode asan` (uses `scripts/analysis/sanitizers.sh` inside container)
- Must exit 0, zero heap-use-after-free, zero buffer overflows

Phase 5: Valgrind uninitialized memory check
- Container: `php83-dev` (has valgrind installed)
- Command: `docker compose exec php83-dev bash -c "USE_ZEND_ALLOC=0 ZEND_DONT_UNLOAD_MODULES=1 /ext/scripts/run-valgrind.sh --quick"`
- Uses suppression file `valgrind-php.supp`
- Must report 0 errors (after suppressions)

Phase 6: clang-tidy and cppcheck static analysis
- Commands: `docker compose exec php83-dev bash -c "/ext/scripts/analysis/clang_tidy.sh"` and `/ext/scripts/analysis/cppcheck.sh`
- Zero new errors vs v9 baseline

[Implementation Order]
Ordered to minimize risk: fix state first, validate, then tag.

1. Delete premature tag locally and on GitHub (`git tag -d v10.0.0` + `git push origin :refs/tags/v10.0.0`)
2. Clean working directory: gitignore *.loT files, delete NEXT_STEPS.md, commit all pending spec/doc modifications
3. Rewrite CHANGELOG.md [10.0.0] section with accurate content only; fix ordering (10 before 9)
4. Close GitHub issue #132 with explanation; move #120 to v11 milestone
5. Update v10.0.0 milestone description to reflect actual scope
6. Commit: `chore: v10.0.0 release audit - accurate changelog, clean state`
7. Run stubs sync check: `bash scripts/check-stubs-sync.sh` - verify exit 0
8. Run core tests: php83-dev/firebird40 - verify 254+/262, 0 failed
9. Run full 12-container matrix: `bash scripts/test_matrix.sh` - note results per container, accept known skips
10. Bring up php83-asan container: `docker compose up -d php83-asan`
11. Run ASAN tests: `./scripts/run-sanitizer.sh` - zero memory errors
12. Run Valgrind quick check: `docker compose exec php83-dev /ext/scripts/run-valgrind.sh --quick` - zero errors after suppressions
13. Run static analysis (clang-tidy, cppcheck) - zero new issues
14. Create clean v10.0.0 tag: `git tag -a v10.0.0 -m "Release v10.0.0 - clean stable: N/262 tests pass, ASAN clean, Valgrind clean"`
15. Push tag: `git push origin v10.0.0`
16. Create PR: `release/v10.0.0` -> `satware-main`
17. Verify GitHub CI passes on the PR
18. Merge PR to satware-main

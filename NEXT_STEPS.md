# Next Steps — php-firebird Post-v7.0.0

**Version**: 7.0.0 (released 2026-03-03)
**Branch**: `satware-main`
**Coverage**: 65.2% (5,089 / 7,802) — measured on `php84-fb3-dev` container

## Priority 1 — Coverage 80% Gate (Issue #58/#63)

Target: ≥80% on `php84-fb3-dev` container before v7.1.0.

```text
File                         Gap    Approach
firebird_utils.cpp           281    Batch API → skip (FB4+ only, not reachable on FB3)
fbird_query_bind.c           146    87 lines dead on FB3 (legacy isc API); 59 coverable
src/cpp/fb_service.hpp       139    Service API tests (fbird_service_* PHP calls)
src/cpp/fb_blob.hpp          104    BlobWrapper::seek, segmented read, unclosed BLOBs
fbird_query_array.c           90    Multi-dim arrays, CHAR/FLOAT/DATE, isc_array_lookup_bounds
fbird_query_exec.c            88    Cursor ops, named resultset, EXECUTE PROCEDURE path
```

## Priority 2 — Near-80% Polish

```text
File                         Gap
src/cpp/fb_events.hpp         56    Event cancel, timeout
src/cpp/fb_statement.hpp      48    Prepare/describe path
fbird_events.c                45    poll timeout === 0, cancel during poll
src/cpp/fb_array.hpp          38    Multi-dim put, typed put
fbird_transaction.c           29    Limbo resolution, multi-DB transactions
fbird_inspection.c            27    Long attachment list, MON$STATEMENTS paths
firebird.c                    24    Remaining ibase_* alias paths
```

## Quick-Win Tests (Phase 2)

1. **`tests/coverage/blob_seek_segments.phpt`** — Cover `fb_blob.hpp` lines 124-232
2. **`tests/coverage/exec_cursor_named.phpt`** — Cover `fbird_query_exec.c` lines 348-477
3. **`tests/coverage/service_all_ops.phpt`** — Cover `fb_service.hpp` (0–20% → 60%+)

## Open Issues

| Issue | Title | Priority |
|-------|-------|---------|
| #59 | Service API coverage (0%→80%) | P0 |
| #60 | Batch operations coverage | P0 |
| #61 | Array operations coverage (35.9%→80%) | P0 |
| #62 | Parameter binding coverage (41.8%→80%) | P0 |
| #63 | Final coverage validation ≥80% | P0 |
| #58 | Code Coverage 80%+ gate | P0 |
| #57 | Source refactoring (firebird.c split) | P1 |
| #67 | Docs cleanup | P2 |
| #68 | Doctrine integration | P1 (after 80% coverage) |

## v7.1.0 Scope (tentative)

- Coverage ≥80% validated on all containers
- `firebird.c` split into smaller modules (Issue #57)
- Doctrine DBAL native driver (Issue #68)
- PHP 8.1 EOL drop
- Firebird 2.5 server EOL drop

## Implementation Plan

Full v7.0.0 implementation plan archived at:
`docs/plans/v7-implementation-plan.md`

# Plan: Connection Simplification — Remove Legacy Dual-Handle Architecture

**Issue**: Connection simplification (internal)
**Spec**: `.specify/specs/connection-simplification/spec.md`
**Date**: 2026-03-21
**Status**: Draft

---

## Constitution Check

| Article | Gate | Status |
|---------|------|--------|
| I: C Extension First | All new files in C (or C++ in `src/cpp/`), registered in config.m4 | ✅ No new source files expected |
| II: Test-First | Failing tests written before implementation | ⬜ Per-phase |
| III: Memory Safety | ASan/UBSan/Valgrind plan defined | ⬜ After each phase |
| VI: Atomic Commits | Each commit ≤200 LOC, single concern | ⬜ Per task |
| VII: Coverage Gate | ≥80% on all touched files | ⬜ After each phase |
| VIII: Docker Testing | Tests run in docker/ environment | ⬜ All phases |

---

## Technical Approach

### Chosen Approach

**Incremental inside-out migration**: Replace legacy handles one struct at a time
(connection → transaction → blob), keeping the extension functional after every
commit. Each phase:

1. Adds/updates an inline helper that extracts the OO pointer
2. Replaces all `handle.*` / `isc_*()` usages in affected files
3. Removes the legacy field from the struct
4. Runs full test suite + sanitizers

The OO API wrappers (`fbc_*`, `fbt_*`, `fbb_*`) already exist and are battle-tested.
This plan only removes the **legacy parallel path**, not adding new functionality.

### Alternatives Considered

| Approach | Pros | Cons | Decision |
|----------|------|------|----------|
| Big-bang rewrite | Single PR, clean diff | High risk, untestable intermediate states | Rejected |
| **Incremental per-struct** | Each commit is testable, bisectable | More commits, longer timeline | **Chosen** |
| Wrapper-only (keep handles, hide behind macros) | Minimal code change | Doesn't actually simplify; dual state remains | Rejected |

---

## Files Affected

| File | Change Type | Description |
|------|-------------|-------------|
| `php_fbird_includes.h` | Modify | Remove `fb_safe_handle` from structs, remove union |
| `src/php_fbird_compat.h` | Modify | Simplify shims to OO-only, add `fbird_get_attachment()` etc. |
| `fbird_connection.c` | Modify | Remove smuggling hack, use `fbc_connection` directly |
| `fbird_transaction.c` | Modify | Replace `handle.tr` with `fbt_transaction` |
| `fbird_blobs.c` | Modify | Replace `bl_handle.blob` with `fbb_blob` |
| `fbird_events.c` | Modify | Replace `isc_wait_for_event()` etc. with OO wrappers |
| `fbird_inspection.c` | Modify | Replace `isc_database_info()` with `fbc_get_info()` |
| `fbird_query_exec.c` | Modify | Replace `handle.db`/`handle.tr` with OO accessors |
| `fbird_query_bind.c` | Modify | Replace `handle.db`/`handle.tr` with OO accessors |
| `fbird_query_array.c` | Modify | Replace `isc_array_*()` with OO API or system table queries |
| `fbird_result.c` | Modify | Replace `stmt` handle usages (where applicable) |
| `firebird.c` | Modify | Replace `handle.db` in gen_id, batch ops |
| `src/cpp/fb_events.hpp` | Create | New C++ wrapper for OO event API |
| `src/cpp/fb_connection.hpp` | Modify | Add `fbc_ping()` / `fbc_is_connected()` if not present |
| `php_fbird_query_array.h` | Modify | Update `_php_fbird_alloc_array()` signature (remove `fb_safe_handle` params) |

---

## Implementation Sequence

### Phase 1: Connection Struct (3 commits)

1. **Add `fbird_get_attachment()` helper** — inline function in `src/php_fbird_compat.h` that extracts `IAttachment*` via `fbc_get_attachment()` → `refactor(compat): add fbird_get_attachment() inline helper`
2. **Rewrite `_php_fbird_attach_db()`** — return `void*` (connection pointer) directly, remove status-vector smuggling, update `_php_fbird_connect()` caller → `refactor(connection): return connection pointer directly from attach`
3. **Remove `handle.db` from `fbird_db_link`** — replace all `handle.db` usages across files with `fbird_get_attachment()`, remove field → `refactor(connection): remove legacy handle.db from fbird_db_link`

### Phase 2: Persistent Ping (1 commit)

4. **Replace `isc_database_info()` in persistent check** — use `fbc_is_connected()` or `fbc_ping()` wrapper using `IAttachment::getInfo()` → `refactor(connection): replace isc_database_info with OO ping`

### Phase 3: Transaction Struct (2 commits)

5. **Add `fbird_get_transaction()` helper** — inline function extracting `ITransaction*` via `fbt_get_handle()` → `refactor(compat): add fbird_get_transaction() inline helper`
6. **Remove `handle.tr` from `fbird_transaction`** — replace all `handle.tr` usages, remove field → `refactor(transaction): remove legacy handle.tr from fbird_transaction`

### Phase 4: Blob Struct (2 commits)

7. **Add `fbird_get_blob()` helper** — inline function extracting `IBlob*` via `fbb_get_handle()` → `refactor(compat): add fbird_get_blob() inline helper`
8. **Remove `bl_handle.blob` from `fbird_blob`** — replace all `bl_handle.blob` usages, remove field → `refactor(blob): remove legacy bl_handle from fbird_blob`

### Phase 5: Events (2 commits)

9. **Create `fb_events.hpp` C++ wrapper** — wrap `IAttachment::queEvents()`, EPB construction, `IUtil::decodeEvents()` with C-callable `fbe_*` functions → `feat(events): add OO event API wrapper in fb_events.hpp`
10. **Replace `isc_*` event calls in `fbird_events.c`** — use `fbe_*` wrappers, replace `isc_free()` with `free()` → `refactor(events): replace legacy isc event functions with OO API`

### Phase 6: Arrays (2 commits)

11. **Add OO array slice wrappers** — C-callable wrappers for `IAttachment::getSlice()`/`putSlice()` in `firebird_utils.cpp` → `feat(array): add OO array slice wrappers`
12. **Replace `isc_array_*()` in `fbird_query_array.c`** — use OO wrappers, replace `isc_array_lookup_bounds()` with system table query → `refactor(array): replace legacy isc_array functions with OO API`

### Phase 7: Cleanup (2 commits)

13. **Remove `fb_safe_handle` union** — delete from `php_fbird_includes.h`, update any remaining references → `refactor(cleanup): remove fb_safe_handle union`
14. **Final verification & compat shim cleanup** — simplify `php_fbird_compat.h`, remove dead code, verify no `isc_*()` function calls remain → `refactor(cleanup): simplify compat shims, verify zero legacy calls`

---

## Test Plan

| Test File | Coverage Target | What It Tests |
|-----------|----------------|---------------|
| `tests/fbird_connect_001.phpt` | `fbird_connect()` | Basic connect/disconnect still works |
| `tests/fbird_pconnect_001.phpt` | `fbird_pconnect()` | Persistent connections with OO ping |
| `tests/fbird_pconnect_limit.phpt` | `fbird_pconnect()` | Max persistent limit enforcement |
| `tests/fbird_transaction_*.phpt` | `fbird_trans()` | Transaction start/commit/rollback |
| `tests/fbird_blob_*.phpt` | `fbird_blob_*()` | Blob create/open/read/write |
| `tests/fbird_event_*.phpt` | `fbird_set_event_handler()` | Event registration and firing |
| `tests/fbird_query_array_*.phpt` | Array operations | Array get/put slice |
| Existing full suite | All functions | Regression — no behavioral changes |

### Sanitizer Validation

```bash
# Run after each phase
docker compose run --rm php83-dev /ext/scripts/run-sanitizer.sh
# Run after final phase
docker compose run --rm php83-dev /ext/scripts/run-valgrind.sh
```

---

## Risks

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| OO event API missing features vs `isc_*` | Medium | High | Research `IEvents` interface before Phase 5; fall back to keeping `isc_wait_for_event` if needed |
| Array slice OO API differs from `isc_array_*` | Low | Medium | `IAttachment::getSlice()`/`putSlice()` are direct replacements per Firebird docs |
| Persistent connection ping behavior change | Low | Medium | Test with stale connections; `IAttachment::getInfo()` is equivalent to `isc_database_info()` |
| FB 2.5 server incompatibility with OO client | Low | Low | FB 3.0 client OO API is designed to work with FB 2.5 servers; verify in CI if FB 2.5 container available |
| Statement handle (`stmt`) left as `fb_safe_handle` | N/A | Low | Explicitly out of scope; documented for future spec |

---

## Definition of Done

- [ ] All tasks in `tasks.md` completed
- [ ] `fb_safe_handle` union removed from `php_fbird_includes.h`
- [ ] Zero `isc_*()` function calls in `.c` files (constants allowed)
- [ ] Zero `handle.db`, `handle.tr`, `bl_handle.blob` references
- [ ] Tests pass in Docker (`make test`) across PHP 8.2-8.5 × FB 3.0-5.0
- [ ] Coverage ≥80% on touched files (lcov)
- [ ] ASan + UBSan clean
- [ ] Valgrind: no definite leaks
- [ ] PR approved and merged to `satware-main`

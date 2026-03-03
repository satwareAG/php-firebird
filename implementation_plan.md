# Implementation Plan: php-firebird v7.0.0-final

[Overview]
Bring php-firebird from v7.0.0-rc.51 to v7.0.0-final by implementing the missing `fbird_query_params_tx` function required by doctrine-firebird-driver, adding missing stubs for existing functions, pushing code coverage from 65.2% to ≥80%, modernizing remaining legacy `isc_*` API calls to the Firebird 3+ OO API, and validating doctrine-firebird-driver integration.

This plan targets three interdependent workstreams executed in sequence: **(A) Doctrine API completeness** — implement `fbird_query_params_tx` and add missing stubs for `fbird_execute_query`, `fbird_execute_statement`, `fbird_execute_auto` (registered in firebird.c but absent from stubs); **(B) Coverage push** — write `.phpt` tests for the six files still below 80%; **(C) Legacy API modernization** — migrate remaining legacy `isc_*` function calls in `fbird_service.c`, `fbird_events.c`, and `fbird_query_array.c` to the FB3+ OO C++ wrapper layer while keeping `isc_tpb_*` and `isc_info_*` constants (which are not deprecated). The Firebird 3+ client library (`libfbclient.so`) is used throughout; the client supports connecting to Firebird 2.5–5.0 servers via wire protocol.

**Compatibility target**: Firebird 2.5–5.0 servers via Firebird 3+ client. The OO API (IAttachment, ITransaction, IStatement, IBlob, IService) is available in all Firebird 3+ clients. Legacy isc_* API calls are deprecated in Firebird 5.0 and will be removed in Firebird 6.0.

**Key constraint**: No public PHP API changes except adding `fbird_query_params_tx`. Backwards-compatible. Zero test regressions.

**Investigation findings**:
- `fbird_query_params_tx` does NOT exist anywhere in the codebase (zero matches) — must be implemented
- `fbird_execute_query`, `fbird_execute_statement`, `fbird_execute_auto` are registered in `firebird.c` (lines 460-462) and declared in `php_firebird.h` (lines 51-53) but are MISSING from both stub files
- `fbsvc_*` wrappers (fbsvc_attach, fbsvc_detach, fbsvc_start, fbsvc_query) are declared in `firebird_utils.h` but only used in cleanup paths in `fbird_service.c`
- `fbe_*` event wrappers present and used only in cleanup; `fba_*` array wrappers declared but not yet used in `fbird_query_array.c`
- 39 tests already in `tests/coverage/` from Phase 1; new tests slot alongside

---

[Types]
No new C-level types required; `fbird_query_params_tx` reuses existing `fbird_db_link`, `fbird_transaction`, `fbird_query` structs.

For modernization of `fbird_service.c` OO path, the existing `fbird_service.fbsvc_service` pointer (void* to `fb::ServiceWrapper*`) is already defined and allocated — no new struct members needed.

---

[Files]

### New/Modified files

| File | Change | Reason |
|------|--------|--------|
| `fbird_query_exec.c` | Add `PHP_FUNCTION(fbird_query_params_tx)` | Missing function used by doctrine |
| `php_firebird.h` | Add `PHP_FUNCTION(fbird_query_params_tx)` declaration | Public header |
| `firebird.c` | Add arginfo + `PHP_FE(fbird_query_params_tx, ...)` to function table | Register function |
| `stubs/firebird-stubs.php` | Add stubs for `fbird_query_params_tx`, `fbird_execute_query`, `fbird_execute_statement`, `fbird_execute_auto` | PHPStan + IDE support — these 3 existing functions have no stubs |
| `phpstan/fbird.stub.php` | Add same 4 function stubs | PHPStan |
| `fbird_service.c` | Migrate `isc_service_attach` / `isc_service_detach` / `isc_service_start` / `isc_service_query` to OO API via existing `fbsvc_*` wrappers | Legacy API removal under `#if FB_API_VER >= 30` |
| `fbird_events.c` | Migrate `isc_event_block`, `isc_free`, `isc_wait_for_event`, `isc_event_counts` to `IEvents` via `src/cpp/fb_events.hpp` | Legacy API removal |
| `fbird_query_array.c` | Migrate `isc_array_lookup_bounds`, `isc_encode_timestamp/date/time` to OO API via `fba_*` wrappers | Legacy API removal |
| `tests/coverage/*.phpt` | Add 6 new test files covering coverage gaps | Reach 80% coverage gate |
| `NEXT_STEPS.md` | Update to reflect Phase 2 completion | Documentation |

### New test files

| File | Coverage target |
|------|----------------|
| `tests/coverage/exec_cursor_named.phpt` | `fbird_query_exec.c` cursor + named resultset paths |
| `tests/coverage/blob_seek_segments.phpt` | `src/cpp/fb_blob.hpp` seek + segmented read paths (already exists, needs validation) |
| `tests/coverage/service_all_ops.phpt` | `fbird_service.c` + `src/cpp/fb_service.hpp` query paths |
| `tests/coverage/events_cancel_timeout.phpt` | `fbird_events.c` cancel + timeout paths |
| `tests/coverage/array_multidim_types.phpt` | `fbird_query_array.c` multi-dim + CHAR/FLOAT/DATE arrays |
| `tests/coverage/query_params_tx.phpt` | `fbird_query_params_tx` new function |

---

[Functions]

### New functions

| Function | File | Signature | Purpose |
|----------|------|-----------|---------|
| `PHP_FUNCTION(fbird_query_params_tx)` | `fbird_query_exec.c` | `fbird_query_params_tx(resource $link, resource $trans, string $sql, array $params): resource\|false` | Execute parameterized query against explicit transaction. Used by doctrine-firebird-driver Connection.php:892. Combines fbird_prepare + fbird_execute into single call for performance. Returns result resource or false on error. |

**Implementation pattern for `fbird_query_params_tx`:**
```c
/* Modeled after fbird_execute_query (lines 1484-1563 in fbird_query_exec.c) */
/* Accepts: link, trans, SQL string, params array */
/* 1. Parse params: zend_parse_parameters "rrsa/" → link_res, trans_res, sql, params_array */
/* 2. Fetch link resource (le_link/le_plink via zend_fetch_resource2_ex) */
/* 3. Fetch trans resource (le_trans via zend_fetch_resource_ex) */
/* 4. Call _php_fbird_prepare() */
/* 5. Convert params HashTable → zval array via _php_fbird_hash_to_zval_array() */
/* 6. Call _php_fbird_exec() */
/* 7. Cleanup bind_args, delete ib_query->res on non-resource result */
/* Return: result resource (SELECT) or false/error */
/* Key difference from fbird_execute_query: accepts explicit $link AND $trans */
```

### Modified functions

| Function | File | Change |
|----------|------|--------|
| `_php_fbird_free_service()` | `fbird_service.c` | Use `fbsvc_detach()` OO path for all detach (not just `#if FB_API_VER >= 30`) |
| `PHP_FUNCTION(fbird_service_attach)` | `fbird_service.c` | Add FB3+ OO attach path via `fbsvc_attach()` |
| `PHP_FUNCTION(fbird_backup)` | `fbird_service.c` | Use OO service wrapper for start/query |
| `PHP_FUNCTION(fbird_restore)` | `fbird_service.c` | Use OO service wrapper |
| `PHP_FUNCTION(fbird_maintain_db)` | `fbird_service.c` | Use OO service wrapper |
| `PHP_FUNCTION(fbird_db_info)` | `fbird_service.c` | Use OO service query |
| `PHP_FUNCTION(fbird_server_info)` | `fbird_service.c` | Use OO service query |
| `_php_fbird_make_event_block()` | `fbird_events.c` | Replace `isc_event_block()` + `isc_free()` with OO IEvents |
| `_php_fbird_wait_and_fill_event()` | `fbird_events.c` | Replace `isc_wait_for_event()` + `isc_event_counts()` with OO |
| `_php_fbird_get_array_descriptor()` | `fbird_query_array.c` | Replace `isc_array_lookup_bounds()` with `fba_lookup_bounds()` OO API wrapper |
| `_php_fbird_write_array()` | `fbird_query_array.c` | Replace `isc_encode_timestamp/date/time` with OO datetime encoding |

---

[Classes]
No class changes. `Firebird\Exception` registration unchanged.

---

[Dependencies]
No new external dependencies.

Internal:
- `fbird_service.c` gains full use of `fbsvc_*` C wrappers (already in `firebird_utils.h` lines 773-842)
- `fbird_events.c` needs to use `fbe_*` wrappers (`fbe_queue`, `fbe_cancel`, `fbe_has_event_fired`, `fbe_get_event_data`) already in `firebird_utils.h` lines 706-759
- `fbird_query_array.c` needs `fba_lookup_bounds`, `fba_get_slice`, `fba_put_slice` from `firebird_utils.h` lines 859-904
- Doctrine-firebird-driver requires `^7.0.0-rc.52` → needs stubs package update post-release

---

[Testing]

All tests run inside Docker via `docker compose -f docker/docker-compose.yml run --rm php84-fb3-dev`.

**Coverage gap targets** (from NEXT_STEPS.md):

| File | Current lines uncovered | Target |
|------|------------------------|--------|
| `fbird_query_exec.c` | 88 | Cursor + EXECUTE PROCEDURE paths |
| `src/cpp/fb_blob.hpp` | 104 | seek, segmented mode, unclosed blobs |
| `fbird_service.c` / `fb_service.hpp` | 139 | OO service query ops |
| `fbird_events.c` | 45 | cancel + timeout |
| `fbird_query_array.c` | 90 | multi-dim, typed arrays |
| `src/cpp/fb_array.hpp` | 38 | typed put operations |

**Note**: `tests/coverage/blob_seek_segments.phpt` already exists in the repo — verify it runs and covers target lines before creating a duplicate.

**Test invocation pattern** (all new tests):
```phpt
--TEST--
fbird_query_params_tx: execute parameterized query with explicit transaction
--SKIPIF--
<?php
require_once __DIR__ . '/../config.inc';
if (!extension_loaded('firebird')) die('skip firebird extension not loaded');
if (!@fbird_connect(FBIRD_TEST_DB, FBIRD_TEST_USER, FBIRD_TEST_PASS)) die('skip cannot connect');
?>
--FILE--
<?php
require_once __DIR__ . '/../config.inc';
// test body
echo "ok\n";
?>
--EXPECT--
ok
```

**Verification commands:**
```bash
# Run new test
docker compose -f docker/docker-compose.yml run --rm php84-fb3-dev /ext/scripts/test.sh tests/coverage/query_params_tx.phpt

# Full coverage
docker compose -f docker/docker-compose.yml run --rm php84-fb3-dev /ext/scripts/coverage.sh

# Check total %
grep -E "^LF:|^LH:" coverage/lcov_filtered.info | \
  awk '/^LF:/{lf+=substr($0,4)} /^LH:/{lh+=substr($0,4)} END{printf "%d/%d = %.1f%%\n",lh,lf,lh/lf*100}'

# Full test matrix
docker compose -f docker/docker-compose.yml run --rm php84-fb3-dev make test

# ASan/UBSan
docker compose -f docker/docker-compose.yml run --rm php84-dev /ext/scripts/run-sanitizer.sh
```

---

[Implementation Order]
Implement in 5 phases, each with its own feature branch, independent PR, and test verification.

**Phase A — `fbird_query_params_tx` + missing stubs (doctrine blocker)**
1. Create `feat/query-params-tx` branch from `satware-main`
2. Add stubs to `stubs/firebird-stubs.php` for `fbird_execute_query`, `fbird_execute_statement`, `fbird_execute_auto` (already registered, missing from stubs)
3. Add same stubs to `phpstan/fbird.stub.php`
4. Add `PHP_FUNCTION(fbird_query_params_tx)` to `fbird_query_exec.c` — modeled after `fbird_execute_query` but with explicit `$link` parameter
5. Add arginfo to `firebird.c`, `PHP_FE` registration, declaration in `php_firebird.h`
6. Add stub to `stubs/firebird-stubs.php` and `phpstan/fbird.stub.php`
7. Write `tests/coverage/query_params_tx.phpt` (RED → GREEN)
8. Run full test matrix, PR to `satware-main`

**Phase B — Coverage push to 80%**
9. Create `test/coverage-phase2` branch
10. Validate `tests/coverage/blob_seek_segments.phpt` runs on FB3 (already exists)
11. Write `tests/coverage/exec_cursor_named.phpt` (cursor + EXECUTE PROCEDURE)
12. Write `tests/coverage/service_all_ops.phpt` (service query ops)
13. Write `tests/coverage/events_cancel_timeout.phpt` (event cancel + timeout)
14. Write `tests/coverage/array_multidim_types.phpt` (multi-dim arrays)
15. Run coverage, verify ≥80%, PR to `satware-main`

**Phase C — fbird_service.c OO API migration**
16. Create `refactor/service-oo-api` branch
17. Migrate `fbird_service_attach` to OO path via `fbsvc_attach()`
18. Migrate `fbird_backup/restore/maintain_db/db_info/server_info` to use `fbsvc_*` wrappers exclusively
19. Remove legacy `isc_service_*` calls (under `#if FB_API_VER >= 30` guard → make unconditional)
20. Run full test matrix, PR to `satware-main`

**Phase D — fbird_events.c OO API migration**
21. Create `refactor/events-oo-api` branch
22. Migrate `isc_event_block` → `fbe_queue()` OO wrapper
23. Migrate `isc_wait_for_event` + `isc_event_counts` → `fbe_*` OO equivalents
24. Remove `isc_free()` calls (replaced by `fbe_free()` in cleanup)
25. Run full test matrix + ASan, PR to `satware-main`

**Phase E — fbird_query_array.c OO migration + release**
26. Create `refactor/array-oo-api` branch
27. Migrate `isc_array_lookup_bounds` → `fba_lookup_bounds()`
28. Migrate `isc_encode_timestamp/date/time` → OO datetime utilities
29. Run full test matrix, coverage final check (must be ≥80%)
30. Bump VERSION to `7.0.0`, tag, GitHub release
31. Update doctrine-firebird-driver `composer.json` to require `ext-firebird: ^7.0.0`

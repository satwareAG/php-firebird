---
description: >-
  v12.0.0 OOP completion: test coverage 13%→>50%, --CLEAN-- for 164+ tests,
  dual-bridge pattern updates, Firebird\Event full implementation (Phase H),
  live event testing, OOP helpers in common.inc, typed arginfo annotations,
  fbird_classes.c split into per-class files, service struct unification.
tags: [oop, testing, event, arginfo, coverage, v12, phase-h, refactor]
priority: 13
---

> **Status: ACTIVE** — v12.0.0 OOP completion milestone. Depends on Phase H event implementation before live event tests.
> **Audit 2026-07-03**: `zend_bool` migration (OC-9) and M6 fuzz dictionary removed - both already done in v11.1.0.

# Spec: v12.0.0 OOP Completion

## Goal

Reach a state where the OOP API (`Firebird\*` namespace) is as well-tested and
documented as the procedural API. Close the gap between the completed C-level M3
migration and the PHP-level test suite, which currently validates only 13% of OOP
surface area. Fully implement the `Firebird\Event` class (Phase H) and establish
patterns for live event testing with actual database triggers.

This is a **non-breaking** milestone release. It adds OOP test infrastructure,
completes the Event class, and hardens argument type information.

## Version

`v12.0.0` — major version for the OOP completion milestone

## Priority

**P3** — planned improvement (v11.1.0 targets that require more design work)

---

## Success Metrics

| Metric | Baseline (v11.0.0) | Target (v12.0.0) |
|--------|-------------------|------------------|
| OOP API test coverage | 10.9% (31/284 tests) | >50% (>142 tests) |
| Tests with `--CLEAN--` | 25.7% (73/284) | >80% (>227 tests) |
| `is_resource()` only test files | 10 files | 0 files |
| `Firebird\Event` methods accessible from PHP | 0 | ≥3 (wait, cancel, getName) |
| Dual-bridge test coverage | 1.4% (4/284) | >10% (>28 tests) |
| Typed arginfo (procedural parameters) | 15/174 typed (8.6%) | 174/174 typed |
| Typed arginfo (OOP method returns) | 0/9 return types | 9/9 return types |

---

## Dependencies

```
Phase H Event Implementation (OC-2)
    ↓
Live Event Testing (OC-3)
    ↓
OOP Coverage Expansion (OC-1)
```

Phase H must be complete and merged before live event tests (`OC-3`) can be written.
All other items (`OC-1`, `OC-4`–`OC-9`) are independent.

---

## Success Criteria

### OC-1: OOP API Test Coverage >50% (T2)

- [ ] New test files added to reach ≥118 OOP-focused tests
- [ ] `Firebird\Statement` multi-row fetch scenarios covered (currently 1 test file)
- [ ] `Firebird\Statement` re-execute after fetch-exhaustion covered
- [ ] `Firebird\Statement` explicit `free()`/`close()` lifecycle covered
- [ ] `Firebird\ResultSet` all fetch modes covered (fetch_row, fetch_assoc, fetch_object, fetch_all)
- [ ] `Firebird\Blob` OOP create/write/close/open/read round-trip covered
- [ ] `Firebird\Transaction` commit/rollback/retain covered via OOP API
- [ ] `Firebird\Connection` negative tests: call method on closed connection → exception
- [ ] Persistent connection (`Firebird\Connection::pconnect()`) round-trip covered
- [ ] Concurrent transaction conflict scenario covered
- [ ] `blob_segfault_after_commit.phpt` updated to use `Firebird\*` objects
- [ ] Coverage script (`coverage.yml`) reaches ≥60% line coverage threshold (was 54%)

### OC-2: `Firebird\Event` Full Implementation — Phase H (C11)

- [ ] `Firebird\Event` class in `fbird_classes.c` registers at minimum these methods:
  - `wait(float $timeout = -1.0): bool` — blocks until event fires or timeout expires
  - `cancel(): bool` — cancels a pending event wait
  - `getName(): string` — returns the event name string
  - `getCount(): int` — returns the number of times the event fired
- [ ] `fbird_set_event_handler($conn, callable $callback, string ...$events): Firebird\Event` unchanged externally
- [ ] `fbird_wait_event(Firebird\Event $event, float $timeout = -1.0): bool` accepts Firebird\Event object
- [ ] `arginfo_fbird_event_*` updated with typed return macros
- [ ] `stubs/firebird-classes.php` updated with `Firebird\Event` method signatures
- [ ] `phpstan/firebird-event.stub.php` is intentionally empty (consolidated into
  `stubs/firebird-classes.php`) - no work required here
- [ ] Phase H completion closes the "registered but has no methods" gap (C11 from QA report)

### OC-3: Live Event Testing with Actual DB Triggers (T4)

**Depends on**: OC-2 (Phase H Event implementation)

- [ ] New test `tests/fbird_event_live_001.phpt` triggers a Firebird `POST_EVENT` via a DDL trigger
- [ ] Test creates a trigger: `CREATE TRIGGER t_post_event AFTER INSERT ON <test_table> AS BEGIN POST_EVENT 'test_event'; END`
- [ ] Test calls `fbird_set_event_handler()` before the INSERT
- [ ] Test performs INSERT and verifies event callback is invoked with `count >= 1`
- [ ] Test uses `Firebird\Event::wait(5.0)` to await the event with a 5-second timeout
- [ ] Test has a complete `--CLEAN--` section dropping the trigger and test table
- [ ] Test has `--SKIPIF--` checking for Firebird server availability + PHP `pcntl` or `fibers` extension
- [ ] `event_poller_wrapper.phpt` updated with a note that it tests PHP-userland strategy pattern only (not live events)

### OC-4: `--CLEAN--` Sections for High-Risk Tests (T1)

- [ ] All tests that `CREATE TABLE` have `--CLEAN--` with matching `DROP TABLE IF EXISTS`
- [ ] Minimum set addressed (from QA report T1):
  - `migration_001.phpt` — `DROP TABLE IF EXISTS TEST_MIGRATION_FORCE`
  - `datatype_001.phpt` — `DROP TABLE IF EXISTS TEST_001`
  - `long_names_001.phpt` — `DROP TABLE IF EXISTS <long-name>`
  - `long_names_002.phpt` — `DROP TABLE IF EXISTS <long-name>`
  - All 7 service tests (`fbird_service_*.phpt`) — cleanup `/tmp/` backup files
  - `fbird_service_oo_002.phpt` — remove backup/restore `/tmp/` files
- [ ] `tests/coverage/` directory: ≥80% of 40 tests have `--CLEAN--` (was 1/40)
- [ ] `grep -rL '\-\-CLEAN\-\-' tests/*.phpt | wc -l` drops from 164 to <47 (80% target)

### OC-5: Dual-Bridge Pattern Updates (T3)

- [ ] All 10 test files that use `is_resource()` without OOP fallback updated to dual-bridge pattern
  (audit 2026-07-03: 10 bare files remain, 4 already use OOP fallback)
- [ ] Files updated (audit 2026-07-03 bare files):
  - `tests/coverage/events_error_handling.phpt`
  - `tests/coverage/inspection_deep.phpt`
  - `tests/fbird_service_001.phpt`
  - `tests/fbird_blob_001.phpt`
  - `tests/issue120.phpt`
  - `tests/test_blob_stream.phpt`
  - `tests/blob_stream_chunked_write.phpt`
  - `tests/bug45575.phpt`
  - `tests/pdo_fbird_blob_handling.phpt` (PDO BLOB handle - may need different pattern than `Firebird\*`)
  - `tests/pdo_fbird_blob_stream.phpt` (PDO BLOB handle - may need different pattern than `Firebird\*`)
- [ ] Dual-bridge pattern used: `assert($result instanceof Firebird\ResultSet || is_resource($result))`
  OR pure OOP assertion where function now guarantees object return
- [ ] New dual-bridge tests target >10% of test suite (>24 tests use both procedural + OOP paths)

### OC-6: OOP Helper Functions in `common.inc` (T6)

- [ ] `tests/common.inc` adds:
  - `oop_connect(string $dsn = null): Firebird\Connection` — OOP equivalent of `db_connect()`
  - `oop_begin_transaction(Firebird\Connection $conn, int $flags = 0): Firebird\Transaction`
  - `oop_query(Firebird\Transaction $tx, string $sql, ...$params): Firebird\ResultSet`
  - `oop_close(Firebird\Connection $conn): void`
- [ ] Helpers respect `tests/config.inc` credentials and DSN settings
- [ ] Helpers are used by at least 10 new OOP test files (not just defined)
- [ ] PHPDoc comments on each helper function for IDE support

### OC-7: Typed Arginfo Annotations — Procedural API (C8)

> **Audit 2026-07-03**: Return types are 100% done (85/85 arginfo blocks in `firebird.c` carry typed returns).
> Only **parameters** remain untyped: 159 of 174 procedural parameters.

- [ ] All 159 untyped `ZEND_ARG_INFO(0, param)` in `firebird.c` replaced with typed variants:
  - Connection handle params: `ZEND_ARG_OBJ_INFO(0, connection, Firebird\\Connection, 0)`
  - Transaction handle params: `ZEND_ARG_OBJ_INFO(0, transaction, Firebird\\Transaction, 0)` (with nullable where dual-accept)
  - String params: `ZEND_ARG_TYPE_INFO(0, param, IS_STRING, 0)`
  - Long params: `ZEND_ARG_TYPE_INFO(0, param, IS_LONG, 0)`
  - Boolean params: `ZEND_ARG_TYPE_INFO(0, param, _IS_BOOL, 0)`
  - Mixed/resource params: `ZEND_ARG_INFO(0, param)` with comment justifying no type
- [ ] Typed params enable better `ReflectionFunction` output and static analysis
- [ ] `php --re fbird` shows typed parameter lists for all 89 functions
- [ ] No existing tests break due to stricter type coercion (all params are IS_LONG/IS_STRING safe to type)

### OC-8: Typed Arginfo Annotations — OOP Layer (C9)

- [ ] All 9 OOP method arginfos in `fbird_classes.c` updated to `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_*`:
  - `arginfo_fbird_connection_beginTransaction` → returns `Firebird\Transaction|false`
  - `arginfo_fbird_connection_prepare` → returns `Firebird\Statement|false`
  - `arginfo_fbird_resultset_fetch` → returns `array|false`
  - `arginfo_fbird_statement_execute` → returns `Firebird\ResultSet|bool`
  - `arginfo_fbird_blob_create` → returns `Firebird\Blob|false`
  - `arginfo_fbird_blob_open` → returns `Firebird\Blob|false`
  - `arginfo_fbird_blob_read` → returns `string|false`
  - (+ 2 additional methods as identified in Phase H event implementation)
- [ ] `stubs/firebird-classes.php` return types match arginfos exactly
- [ ] `php --re fbird` shows return types on all OOP methods

### OC-9: `zend_bool` → `bool` Migration (C7) — DONE

> **Status**: Shipped in v11.1.0 (2026-07-02). Zero `zend_bool` occurrences remain.
> This criterion is retained for historical reference; no v12 work required.

### OC-10: RPR_VALIDATE_DB + RPR_MEND_DB Teardown Pattern (T5)

- [ ] `tests/fbird_service_db_mgr.phpt` updated to call `FBIRD_RPR_MEND_DB` after `FBIRD_RPR_VALIDATE_DB`
- [ ] Teardown sequence in `--CLEAN--`: `VALIDATE_DB` → if errors → `MEND_DB` → re-validate
- [ ] Pattern documented in `tests/README.md` (or `tests/common.inc` comment block)
- [ ] At least one new service test demonstrates the full validate→mend→verify cycle

### OC-11: Service Struct Unification (carry-over from QH-4c)

> **Carry-over**: QH-4c in `spec-v11.1-quality-hardening.md` targeted this for v11.1.0
> but it was not implemented. Two structs still exist as of 2026-07-03 audit.

- [ ] `fbird_service` (defined in `php_fbird_includes.h:163`) and `fbird_service_obj`
  (defined in `fbird_classes.c:874`) unified into single canonical struct
- [ ] Shared struct moved to `fbird_service_types.h` included by both files
- [ ] Raw pointer casts between the two types eliminated
- [ ] Note: v11.1 spec referenced `fbird_service_rsrc` - actual name is `fbird_service_obj`
- [ ] Service OOP tests pass without modification

### OC-12: `fbird_classes.c` Split (Structural Refactor)

> **Prerequisite for OC-2** (Phase H Event). Reduces 1450-line monolith to per-class files
> matching the `fbird_<area>.c` Layer 1 naming pattern.

- [ ] Create 8 new files: `fbird_class_{connection,transaction,statement,resultset,blob,batch,service,event}.c`
- [ ] Reduce `fbird_classes.c` to registry only (`fbird_register_classes()` + class entry pointers)
- [ ] `fbird_classes.h` unchanged (public API stable)
- [ ] Move each class's: handlers, free_obj, methods array, `PHP_METHOD` implementations, arginfo
- [ ] Update `config.m4` and `config.w32` with new source files
- [ ] `make test` passes unchanged; ASAN clean

---

## User Stories

### US-OC-1: OOP coverage parity (OC-1)
> As a PHP developer adopting the `Firebird\*` OOP API, I need a test suite that validates
> the OOP path as thoroughly as the procedural path. With only 13% OOP coverage, regressions
> in `Firebird\Statement` or `Firebird\ResultSet` can ship silently.

### US-OC-2: Functional `Firebird\Event` (OC-2)
> As a developer building event-driven applications with Firebird triggers, I need to call
> `$event->wait(5.0)` and `$event->cancel()` from PHP. The class is registered but has
> zero methods — it is unusable from PHP userland until Phase H is complete.

### US-OC-3: Real event integration test (OC-3)
> As a CI engineer, I need a test that fires an actual Firebird POST_EVENT via a database
> trigger and verifies the PHP event callback receives it. The current event tests only test
> the PHP strategy pattern wrapper, not the underlying Firebird C API.

### US-OC-4: Clean test state (OC-4)
> As a developer running `make test` multiple times, tests that leave behind `TEST_001`
> tables or `/tmp/*.fbk` files cause subsequent runs to fail or behave differently.
> Every test that creates state must clean it up.

### US-OC-5: IDE-aware OOP helpers (OC-6)
> As a test author writing a new OOP test, I should be able to call `oop_connect()` from
> `common.inc` and get a properly typed `Firebird\Connection` object, consistent with how
> procedural tests call `db_connect()`. Having to repeat the connection boilerplate in
> every test file discourages OOP test writing.

### US-OC-6: Typed PHP reflection (OC-7/OC-8)
> As a developer calling `(new ReflectionFunction('fbird_query'))->getParameters()`, I
> should see typed parameters, not just `$param_0`, `$param_1`. Typed arginfos enable
> IDEs, static analyzers, and documentation generators to produce accurate information.

### US-OC-7: Clean validate/repair cycle (OC-10)
> As a CI engineer, after a test run that calls `FBIRD_RPR_VALIDATE_DB` and marks the
> database as "damaged", the next test run must not inherit that damaged state. The
> teardown pattern must always pair validate with mend.

---

## Migration Path: Procedural → OOP Testing

Tests should follow this migration pattern when updating from procedural to OOP or dual-bridge:

```php
/* BEFORE (procedural-only): */
$conn = ibase_connect(...);  // or fbird_connect()
$this->assertTrue(is_resource($conn));

/* AFTER (dual-bridge): */
$conn = fbird_connect(...);
$this->assertInstanceOf(Firebird\Connection::class, $conn);
// OR for transition period:
$this->assertTrue($conn instanceof Firebird\Connection || is_resource($conn));

/* PREFERRED (OOP-native): */
$conn = new Firebird\Connection($dsn, $user, $pass);
$this->assertInstanceOf(Firebird\Connection::class, $conn);
```

OOP helper usage pattern (after OC-6 completes):
```php
// In test --FILE-- section:
require_once __DIR__ . '/common.inc';
$conn = oop_connect();
$tx   = oop_begin_transaction($conn);
$rs   = oop_query($tx, 'SELECT 1 FROM RDB$DATABASE');
// ... assertions ...
oop_close($conn);
```

---

## Phase H Implementation Notes (OC-2)

The `Firebird\Event` class is registered in `fbird_classes.c:1394` with `NULL` methods.
Phase H completion requires:

1. **Define event methods** in `fbird_classes.c`:
   ```c
   static const zend_function_entry fbird_event_methods[] = {
       PHP_ME(Firebird_Event, wait,     arginfo_fbird_event_wait,     ZEND_ACC_PUBLIC)
       PHP_ME(Firebird_Event, cancel,   arginfo_fbird_event_cancel,   ZEND_ACC_PUBLIC)
       PHP_ME(Firebird_Event, getName,  arginfo_fbird_event_get_name, ZEND_ACC_PUBLIC)
       PHP_ME(Firebird_Event, getCount, arginfo_fbird_event_get_count,ZEND_ACC_PUBLIC)
       PHP_FE_END
   };
   ```

2. **Route procedural `fbird_wait_event`** to the same internal C function used by `Event::wait()`

3. **Update class registration**:
   ```c
   fbird_event_ce = register_class_Firebird_Event();  // ← replace NULL methods
   ```

4. **Stub update** — `phpstan/firebird-event.stub.php` is currently empty; populate it fully

---

## Affected Files

| File | Change |
|------|--------|
| `fbird_classes.c` | Phase H event methods, OOP arginfo typed returns |
| `firebird.c` | Typed arginfo for 159 procedural parameters |
| `fbird_class_event.c` | New: Phase H event methods (after OC-12 split) |
| `fbird_class_service.c` | New: service struct unification (after OC-12 split) |
| `fbird_service_types.h` | New: unified service struct (OC-11) |
| `tests/common.inc` | Add OOP helpers |
| `tests/*.phpt` (164+) | Add --CLEAN-- sections |
| `tests/*.phpt` (10 files) | Update is_resource() → dual-bridge |
| `tests/fbird_event_live_001.phpt` | New: live event test |
| `tests/fbird_service_db_mgr.phpt` | Add RPR_MEND_DB teardown |
| `stubs/firebird-classes.php` | Phase H event method stubs |
| `config.m4` | Add new per-class source files, LTO flag |

---

## Risks

| Risk | Mitigation |
|------|------------|
| Phase H event implementation is complex async C work | Scope to synchronous `wait()` with timeout first; async remains in v13 |
| Adding --CLEAN-- to 164 tests is mechanical but tedious | Script-assist: generate DROP statements from CREATE TABLE patterns in test files |
| Typed arginfo may break callers using wrong types silently coerced | All affected types are IS_STRING/IS_LONG which PHP coerces from other scalars; run full CI |
| Live event test flaky in CI (timing-dependent) | Use generous timeout (5s), retry on timeout, skip in low-resource CI if needed |
| OOP helpers in common.inc changes shared test infrastructure | Keep helpers additive; no existing function signatures changed |

## Test Strategy

1. **OC-1**: Each new OOP test file follows standard PHPT format with `--SKIPIF--` + `--CLEAN--`
2. **OC-2**: Phase H smoke test: `fbird_event_methods.phpt` checking `method_exists(Firebird\Event, 'wait')`
3. **OC-3**: `fbird_event_live_001.phpt` requires `FIREBIRD_HAS_EVENTS` env var set in CI
4. **OC-4**: Automated audit script `scripts/check-clean-sections.sh` added to `code-quality.yml`
5. **OC-7/8**: `ReflectionFunction` test added verifying typed parameters on 10 representative functions
6. **OC-10**: `fbird_service_db_mgr.phpt` updated, new `fbird_service_validate_mend_cycle.phpt`

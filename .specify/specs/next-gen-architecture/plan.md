# Next-Gen Architecture — Implementation Plan

> Date: 2026-03-22
> Approach: Phased, incremental — each phase is independently shippable

---

## Phase A: Complete isc_* Elimination (Layer 1 Modernization)

**Goal**: Remove all remaining `isc_*` function calls and `fb_safe_handle` union.

### A1: Service API Migration (fbird_service.c)
- Add extern C bridge functions in `firebird_utils.cpp` wrapping `fb_service.hpp`
- Replace `isc_service_attach/detach/start/query` with `fbs_*` wrappers
- Remove `isc_svc_handle` usage from `fbird_service.c`
- Keep `isc_vax_integer()` for SPB/info buffer parsing (not deprecated)

### A2: Statement Handle Migration (fbird_query_*.c)
- Add `fbs_prepare()`, `fbs_execute()`, `fbs_open_cursor()` C bridge functions
- Replace `isc_dsql_prepare/execute/fetch/free` in `fbird_query_prepare.c`
- Replace `isc_dsql_execute_immediate` in `fbird_query_exec.c`
- Migrate `_ib_query.stmt` from `fb_safe_handle` to `void *fbs_statement`
- Remove `fb_safe_handle` union from `php_fbird_includes.h`

### A3: Minor Cleanup
- Replace `isc_sqlcode()` with `IStatus`-based extraction in `fbird_error.c`
- Replace `isc_encode_*` with `IUtil` equivalents in `fbird_query_array.c`
- Keep `isc_wait_for_event()` (async events deferred to Phase B)

### Validation Gate A
- Full 12-target test matrix green
- ASan 3/3 PASS
- Valgrind 0 definitely/indirectly lost
- No `isc_*` function calls remain (except `isc_vax_integer` and constants)

---

## Phase B: Layer 2 OOP Classes

**Goal**: Register `Firebird\*` PHP classes via `zend_class_entry` in C.

### B1: Infrastructure
- Create `fbird_classes.c` / `fbird_classes.h` for class registration
- Register `Firebird\Exception` hierarchy (3 exception classes)
- Add `Firebird\` namespace constant registration
- Update `config.m4` to compile new source files

### B2: Connection Class
- Register `Firebird\Connection` with `__construct`, `close`, `ping`, `isConnected`
- Implement `beginTransaction`, `getDefaultTransaction`
- Implement `prepare`, `execute`, `query`
- Implement `getServerVersion`, `getDatabaseInfo`, `getInfo`
- Add `.phpt` tests for each method

### B3: Transaction Class
- Register `Firebird\Transaction` with commit/rollback/retaining methods
- Implement `isActive`, `getInfo`
- Implement savepoint methods (SQL-based)

### B4: Statement + ResultSet Classes
- Register `Firebird\Statement` with `execute`, `openCursor`, `getPlan`, `getType`
- Register `Firebird\ResultSet` implementing `IteratorAggregate`, `Countable`
- Implement all fetch methods (next, assoc, object, all, column)
- Implement scrollable cursor methods (first, last, absolute, relative)

### B5: Blob + Batch Classes
- Register `Firebird\Blob` implementing `Stringable`
- Register `Firebird\Batch` (FB4+ only, guarded by `#if FB_API_VER >= 40`)
- Implement blob read/write/info methods
- Implement batch add/execute/cancel methods

### B6: Service + Events Classes
- Register `Firebird\Service` wrapping backup/restore/user management
- Register `Firebird\Events` for async event handling
- Implement `Connection::waitForEvent` and `createEventHandler`

### Validation Gate B
- Full 12-target test matrix green (Layer 1 + Layer 2 tests)
- ASan clean, Valgrind clean
- Stubs updated in `stubs/firebird-classes.php`
- All Layer 2 classes have ≥80% test coverage

---

## Phase C: Layer 3 PDO Driver

**Goal**: Create `pdo_fbird.so` as a separate shared module.

### C1: PDO Driver Skeleton
- Create `pdo_fbird/` subdirectory with `config.m4`, `pdo_fbird.c`
- Register `fbird:` DSN prefix
- Implement `pdo_fbird_handle_factory` (connection)
- Implement `pdo_fbird_handle_closer` (disconnect)

### C2: Statement Implementation
- Implement `pdo_fbird_stmt_execute` (prepare + execute)
- Implement `pdo_fbird_stmt_fetch` (fetch row)
- Implement `pdo_fbird_stmt_describe` (column metadata)
- Implement parameter binding

### C3: Transaction Support
- Implement `pdo_fbird_handle_begin` / `commit` / `rollback`
- Map PDO transaction isolation levels to Firebird TPB

### C4: Attributes + Error Handling
- Implement `pdo_fbird_handle_get_attribute` / `set_attribute`
- Map Firebird errors to PDO error codes (SQLSTATE)
- Register `PDO::FBIRD_ATTR_*` constants

### Validation Gate C
- PDO driver passes PDO standard test suite
- Firebird-specific PDO tests pass
- No collision with bundled `pdo_firebird` driver
- Full matrix green

---

## Phase D: Documentation + Release

**Goal**: Complete documentation, stubs, and release preparation.

### D1: Stubs + PHPStan
- Update `stubs/firebird-classes.php` with all Layer 2 classes
- Create `stubs/pdo-fbird-stubs.php` for Layer 3
- Verify PHPStan level 9 passes against stubs

### D2: Documentation
- Update `README.md` with Layer 2 and Layer 3 usage examples
- Create `docs/oop-api.md` with full Layer 2 reference
- Create `docs/pdo-driver.md` with Layer 3 reference
- Update `CHANGELOG.md`

### D3: Release
- Version bump to 8.0.0 (major: new OOP API)
- Full CI matrix green
- All sanitizers clean
- Coverage ≥80%
- Tag and release

---

## Timeline Estimate

| Phase | Scope | Estimated Effort |
|-------|-------|-----------------|
| A | isc_* elimination | 2-3 weeks |
| B | OOP classes | 4-6 weeks |
| C | PDO driver | 2-3 weeks |
| D | Docs + release | 1 week |
| **Total** | | **9-13 weeks** |

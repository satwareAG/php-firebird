# Next-Gen Architecture — Task Breakdown

> Date: 2026-03-22
> Rule: Each task ≤200 LOC changed, test-first per constitution Article II

---

## Phase A: Complete isc_* Elimination

### A1: Service API Migration

- [ ] **A1.1** Write `.phpt` tests for service attach/detach/start/query via OO API
  - `tests/fbird_service_oo_001.phpt` — attach + server version query
  - `tests/fbird_service_oo_002.phpt` — backup/restore round-trip
  - Verify tests FAIL (RED) before implementation

- [ ] **A1.2** Add extern C bridge functions in `firebird_utils.cpp`
  - `fbsvc_attach()` — wraps `IProvider::attachServiceManager()`
  - `fbsvc_detach()` — wraps `IService::detach()`
  - `fbsvc_start()` — wraps `IService::start()`
  - `fbsvc_query()` — wraps `IService::query()`
  - Declare in `firebird_utils.h`

- [ ] **A1.3** Replace `isc_service_attach/detach` in `fbird_service.c`
  - Replace `isc_service_attach()` → `fbsvc_attach()`
  - Replace `isc_service_detach()` → `fbsvc_detach()`
  - Update `php_fbird_service_t` struct if needed

- [ ] **A1.4** Replace `isc_service_start/query` in `fbird_service.c`
  - Replace 3× `isc_service_start()` → `fbsvc_start()`
  - Replace 1× `isc_service_query()` → `fbsvc_query()`
  - Remove `isc_svc_handle` casts

- [ ] **A1.5** Verify A1 tests pass (GREEN), run matrix + ASan

### A2: Statement Handle Migration

- [ ] **A2.1** Write `.phpt` regression tests for statement lifecycle
  - `tests/fbird_stmt_oo_001.phpt` — prepare + execute + fetch cycle
  - `tests/fbird_stmt_oo_002.phpt` — prepared statement reuse
  - `tests/fbird_stmt_oo_003.phpt` — statement free + use-after-free guard
  - Verify existing tests still pass

- [ ] **A2.2** Add `fbs_prepare()` C bridge in `firebird_utils.cpp`
  - Wraps `IAttachment::prepare()` returning `IStatement*`
  - Add `fbs_get_type()`, `fbs_get_plan()`, `fbs_get_affected_records()`
  - Declare in `firebird_utils.h`

- [ ] **A2.3** Add `fbs_execute()` and `fbs_open_cursor()` C bridges
  - `fbs_execute()` — wraps `IStatement::execute()`
  - `fbs_open_cursor()` — wraps `IStatement::openCursor()`
  - `fbs_fetch_next()` — wraps `IResultSet::fetchNext()`
  - `fbs_close_cursor()` — wraps `IResultSet::close()`

- [ ] **A2.4** Add `fbs_free()` and metadata C bridges
  - `fbs_free()` — wraps `IStatement::free()` / `deprecatedFree()`
  - `fbs_get_input_metadata()` — wraps `IStatement::getInputMetadata()`
  - `fbs_get_output_metadata()` — wraps `IStatement::getOutputMetadata()`

- [ ] **A2.5** Migrate `fbird_query_prepare.c` to OO API
  - Replace `isc_dsql_allocate_statement()` + `isc_dsql_prepare()` → `fbs_prepare()`
  - Store `IStatement*` in `_ib_query.fbs_statement`
  - Keep XSQLDA for now (metadata migration in A2.8)

- [ ] **A2.6** Migrate `fbird_query_exec.c` execute path to OO API
  - Replace `isc_dsql_execute()` / `isc_dsql_execute2()` → `fbs_execute()`
  - Replace `isc_dsql_execute_immediate()` → `IAttachment::execute()`
  - Handle both prepared and immediate execution

- [ ] **A2.7** Migrate `fbird_query_exec.c` fetch path to OO API
  - Replace `isc_dsql_fetch()` → `fbs_fetch_next()`
  - Replace `isc_dsql_sql_info()` → `IStatement::getInfo()`
  - Replace `isc_dsql_free_statement()` → `fbs_free()`

- [ ] **A2.8** Migrate to IMessageMetadata for parameter binding
  - Replace XSQLDA input metadata with `IMessageMetadata` from `fbs_get_input_metadata()`
  - Update `fbird_query_bind.c` to use message buffer offsets
  - Keep XSQLDA output for now (result migration in A2.9)

- [ ] **A2.9** Migrate to IMessageMetadata for result fetching
  - Replace XSQLDA output metadata with `IMessageMetadata` from `fbs_get_output_metadata()`
  - Update `fbird_result.c` to use message buffer offsets
  - Remove XSQLDA allocation/deallocation

- [ ] **A2.10** Remove `fb_safe_handle` union
  - Remove `fb_safe_handle` from `php_fbird_includes.h`
  - Remove `_ib_query.stmt` field (replaced by `fbs_statement`)
  - Update all references

- [ ] **A2.11** Verify A2 tests pass (GREEN), run matrix + ASan + Valgrind

### A3: Minor Cleanup

- [ ] **A3.1** Replace `isc_sqlcode()` in `fbird_error.c`
  - Use `IStatus::getErrors()` to extract SQLCODE
  - Keep backward-compatible `fbird_errcode()` return value

- [ ] **A3.2** Replace `isc_encode_*` in `fbird_query_array.c`
  - Replace `isc_encode_timestamp()` → inline or `IUtil` wrapper
  - Replace `isc_encode_sql_date()` → inline or `IUtil` wrapper
  - Replace `isc_encode_sql_time()` → inline or `IUtil` wrapper

- [ ] **A3.3** Fix split-stubs workflow
  - Update `.github/workflows/split-stubs.yml` to force-tag or skip existing

### Validation Gate A
- [ ] **A.VG** Full 12-target matrix + ASan + Valgrind clean

---

## Phase B: Layer 2 OOP Classes

### B1: Infrastructure

- [ ] **B1.1** Create `fbird_classes.c` skeleton + `Firebird\Exception` hierarchy
  - Register `Firebird\Exception extends \RuntimeException`
  - Register `Firebird\ConnectionException extends Firebird\Exception`
  - Register `Firebird\QueryException extends Firebird\Exception`
  - Register `Firebird\ServiceException extends Firebird\Exception`
  - Update `config.m4` to compile `fbird_classes.c`

- [ ] **B1.2** Write `.phpt` tests for exception classes
  - Verify class hierarchy, instanceof checks, message/code propagation

### B2: Connection Class

- [ ] **B2.1** Write `.phpt` tests for `Firebird\Connection` constructor + close
- [ ] **B2.2** Register `Firebird\Connection` class, implement `__construct`, `close`
- [ ] **B2.3** Write `.phpt` tests for `ping`, `isConnected`, `getServerVersion`
- [ ] **B2.4** Implement `ping`, `isConnected`, `getServerVersion`, `getDatabaseInfo`
- [ ] **B2.5** Write `.phpt` tests for `prepare`, `execute`, `query`
- [ ] **B2.6** Implement `prepare`, `execute`, `query`
- [ ] **B2.7** Write `.phpt` tests for `beginTransaction`, `getDefaultTransaction`
- [ ] **B2.8** Implement `beginTransaction`, `getDefaultTransaction`

### B3: Transaction Class

- [ ] **B3.1** Write `.phpt` tests for `Firebird\Transaction` commit/rollback
- [ ] **B3.2** Register + implement `Firebird\Transaction`
- [ ] **B3.3** Write `.phpt` tests for savepoint methods
- [ ] **B3.4** Implement savepoint methods

### B4: Statement + ResultSet

- [ ] **B4.1** Write `.phpt` tests for `Firebird\Statement` execute + metadata
- [ ] **B4.2** Register + implement `Firebird\Statement`
- [ ] **B4.3** Write `.phpt` tests for `Firebird\ResultSet` fetch methods
- [ ] **B4.4** Register + implement `Firebird\ResultSet` (fetchNext, fetchAssoc, etc.)
- [ ] **B4.5** Write `.phpt` tests for scrollable cursor methods
- [ ] **B4.6** Implement scrollable cursor methods + `IteratorAggregate`
- [ ] **B4.7** Write `.phpt` tests for `Countable` interface
- [ ] **B4.8** Implement `Countable::count()`

### B5: Blob + Batch

- [ ] **B5.1** Write `.phpt` tests for `Firebird\Blob`
- [ ] **B5.2** Register + implement `Firebird\Blob` (Stringable)
- [ ] **B5.3** Write `.phpt` tests for `Firebird\Batch` (FB4+ only)
- [ ] **B5.4** Register + implement `Firebird\Batch`

### B6: Service + Events

- [ ] **B6.1** Write `.phpt` tests for `Firebird\Service`
- [ ] **B6.2** Register + implement `Firebird\Service`
- [ ] **B6.3** Write `.phpt` tests for `Firebird\Events`
- [ ] **B6.4** Register + implement `Firebird\Events`

### Validation Gate B
- [ ] **B.VG** Full matrix + ASan + Valgrind + coverage ≥80%

---

## Phase C: Layer 3 PDO Driver

### C1: Driver Skeleton

- [ ] **C1.1** Create `pdo_fbird/` directory structure + `config.m4`
- [ ] **C1.2** Write `.phpt` tests for PDO connection (`fbird:` DSN)
- [ ] **C1.3** Implement `pdo_fbird_handle_factory` + `closer`
- [ ] **C1.4** Register `fbird:` driver with PDO

### C2: Statements

- [ ] **C2.1** Write `.phpt` tests for PDO prepare + execute + fetch
- [ ] **C2.2** Implement `pdo_fbird_stmt_execute` + `fetch`
- [ ] **C2.3** Write `.phpt` tests for parameter binding
- [ ] **C2.4** Implement parameter binding (named + positional)
- [ ] **C2.5** Write `.phpt` tests for column metadata
- [ ] **C2.6** Implement `pdo_fbird_stmt_describe`

### C3: Transactions

- [ ] **C3.1** Write `.phpt` tests for PDO transactions
- [ ] **C3.2** Implement begin/commit/rollback + isolation level mapping

### C4: Attributes + Errors

- [ ] **C4.1** Write `.phpt` tests for PDO attributes
- [ ] **C4.2** Implement get/set attribute + SQLSTATE error mapping
- [ ] **C4.3** Register `PDO::FBIRD_ATTR_*` constants

### Validation Gate C
- [ ] **C.VG** PDO standard tests + Firebird-specific tests + full matrix

---

## Phase D: Documentation + Release

- [ ] **D1.1** Update `stubs/firebird-classes.php` with Layer 2 classes
- [ ] **D1.2** Create `stubs/pdo-fbird-stubs.php` for Layer 3
- [ ] **D1.3** Verify PHPStan level 9 passes
- [ ] **D2.1** Create `docs/oop-api.md` with Layer 2 reference
- [ ] **D2.2** Create `docs/pdo-driver.md` with Layer 3 reference
- [ ] **D2.3** Update `README.md` + `CHANGELOG.md`
- [ ] **D3.1** Version bump to 8.0.0, tag, release

### Validation Gate D
- [ ] **D.VG** Full matrix + ASan + Valgrind + coverage ≥80% + docs complete

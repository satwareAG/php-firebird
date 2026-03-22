# Phase C Tasks — Legacy Debt Cleanup + PDO Driver Skeleton

**Constitution**: Article II (test-first), Article VI (≤200 LOC/commit), Article VIII (Docker testing)

---

## Part 1 — Legacy Debt Cleanup

### T1: Replace `isc_sqlcode()` with IStatus-based extraction (~80 LOC)
- [ ] **T1.1** Add `fbc_sqlcode(void* status_array)` C bridge in `firebird_utils.cpp` / `firebird_utils.h`
  - Parse `IStatus::getErrors()` vector for `isc_arg_gds` code, map to SQLCODE
  - Fallback: use `fb_interpret()` if no direct mapping available
- [ ] **T1.2** Replace `isc_sqlcode(IB_STATUS)` in `fbird_error.c:182` with `fbc_sqlcode()`
- [ ] **T1.3** Run tests: `scripts/test_matrix.sh` (single target) + ASan

### T2: Replace `isc_encode_*()` with IUtil wrappers (~100 LOC)
- [ ] **T2.1** Add `fbu_encode_date()`, `fbu_encode_time()`, `fbu_encode_timestamp()` C bridges
  in `firebird_utils.cpp` wrapping `IUtil::encodeDate()`, `IUtil::encodeTime()`
- [ ] **T2.2** Replace 3× `isc_encode_*()` calls in `fbird_query_array.c:391,441,459`
- [ ] **T2.3** Run tests: array-related `.phpt` tests + ASan

### T3: Replace `isc_array_lookup_bounds()` with OO query (~150 LOC)
- [ ] **T3.1** Add `fba_lookup_bounds()` C bridge in `firebird_utils.cpp` that queries
  `RDB$FIELD_DIMENSIONS` + `RDB$FIELDS` system tables via `IAttachment::execute()`
- [ ] **T3.2** Replace `isc_array_lookup_bounds()` in `fbird_query_array.c:88` with `fba_lookup_bounds()`
- [ ] **T3.3** Run tests: `tests/fbird_array_*.phpt` + full single-target matrix

### T4: Remove `bl_handle` from `fbird_blob` (~60 LOC)
- [ ] **T4.1** Remove `bl_handle.ptr = 0` assignments in `fbird_result.c:736` and `fbird_query_bind.c:858`
- [ ] **T4.2** Remove `fb_safe_handle bl_handle` field from `fbird_blob` in `php_fbird_includes.h:156`
- [ ] **T4.3** Fix any remaining struct initializer mismatches (e.g., `fbird_blobs.c` literals)
- [ ] **T4.4** Run tests: blob `.phpt` tests + ASan

### T5: Remove `handle` from `fbird_db_link` and `fbird_transaction` (~180 LOC)
- [ ] **T5.1** Replace all `link->handle.ptr` / `link->handle.db` usages in `fbird_connection.c`,
  `fbird_events.c`, `fbird_transaction.c` with `fbird_get_attachment()` / `fbc_is_connected()`
- [ ] **T5.2** Replace all `trans->handle.ptr` / `trans->handle.tr` usages in `fbird_transaction.c`,
  `fbird_query_exec.c`, `fbird_connection.c` with `fbird_get_transaction()` / `fbt_is_active()`
- [ ] **T5.3** Remove `fb_safe_handle handle` from `fbird_db_link` (`php_fbird_includes.h:122`)
  and `fbird_transaction` (`php_fbird_includes.h:140`)
- [ ] **T5.4** Update `fbird_link_is_valid()` and `fbird_trans_is_valid()` in `src/php_fbird_compat.h`
- [ ] **T5.5** Run tests: full single-target matrix + ASan + Valgrind

### T6: Remove `_ib_query.stmt` — migrate to OO statement wrapper (~200 LOC)
- [ ] **T6.1** Add `fbs_get_handle()` bridge returning raw `isc_stmt_handle` from `fbs_statement`
  (temporary shim for `isc_dsql_describe()` / `isc_dsql_execute()` callers)
- [ ] **T6.2** Replace `ib_query->stmt.stmt` usages in `fbird_query_prepare.c`, `fbird_query_exec.c`,
  `fbird_query_bind.c`, `fbird_result.c` with `fbs_get_handle(ib_query->fbs_statement)`
- [ ] **T6.3** Remove `fb_safe_handle stmt` from `_ib_query` in `php_fbird_includes.h:224`
- [ ] **T6.4** Run tests: full single-target matrix + ASan

### T7: Remove `fb_safe_handle` union (~20 LOC)
- [ ] **T7.1** Verify `grep -rn 'fb_safe_handle' --include="*.c" --include="*.h"` returns 0 results
- [ ] **T7.2** Remove `fb_safe_handle` typedef from `php_fbird_includes.h:115-119`
- [ ] **T7.3** Remove `fb_safe_handle` from `php_fbird_query_array.h:23` function signature
- [ ] **T7.4** Run: full 12-target matrix + ASan + Valgrind (Validation Gate Part 1)

---

## Part 2 — PDO Driver Skeleton

### T8: Directory structure and build system (~100 LOC)
- [ ] **T8.1** Create `pdo_fbird/config.m4` with `--with-pdo-fbird` option, dependency on `firebird` extension
- [ ] **T8.2** Create `pdo_fbird/config.w32` for Windows builds
- [ ] **T8.3** Create `pdo_fbird/php_pdo_fbird.h` with module entry, version defines
- [ ] **T8.4** Create `pdo_fbird/php_pdo_fbird_int.h` with internal structs

### T9: Module init and DSN registration (~80 LOC)
- [ ] **T9.1** Create `pdo_fbird/pdo_fbird.c` — `PHP_MINIT_FUNCTION`, register `fbird:` driver
- [ ] **T9.2** Write RED test: `pdo_fbird/tests/pdo_fbird_001.phpt` — `new PDO('fbird:...')`
- [ ] **T9.3** Verify test fails, then implement, verify GREEN

### T10: Connection factory (~150 LOC)
- [ ] **T10.1** Create `pdo_fbird/pdo_fbird_driver.c` — DSN parsing (`host`, `dbname`, `charset`, `dialect`, `role`)
- [ ] **T10.2** Implement `pdo_fbird_handle_factory()` calling `fbc_connect()`
- [ ] **T10.3** Implement `pdo_fbird_handle_closer()` calling `fbc_disconnect()`
- [ ] **T10.4** Write test: connect + `getAttribute(PDO::ATTR_SERVER_VERSION)`

### T11: Transaction support (~100 LOC)
- [ ] **T11.1** Implement `pdo_fbird_handle_begin()` / `commit()` / `rollback()` via `fbt_*` bridges
- [ ] **T11.2** Support `PDO::ATTR_AUTOCOMMIT` mode
- [ ] **T11.3** Write test: `beginTransaction()` + `commit()` + `rollback()`

### T12: Statement prepare and execute (~180 LOC)
- [ ] **T12.1** Create `pdo_fbird/pdo_fbird_stmt.c`
- [ ] **T12.2** Implement `pdo_fbird_stmt_preparer()` via `fbs_prepare()`
- [ ] **T12.3** Implement `pdo_fbird_stmt_execute()` via `fbs_execute()`
- [ ] **T12.4** Implement parameter binding (named `:param` and positional `?`)
- [ ] **T12.5** Write test: prepared statement with parameters

### T13: Fetch methods (~150 LOC)
- [ ] **T13.1** Implement `pdo_fbird_stmt_fetch()` via `fbs_fetch_next()`
- [ ] **T13.2** Implement `pdo_fbird_stmt_describe()` for column metadata
- [ ] **T13.3** Implement `pdo_fbird_stmt_get_col()` for column value retrieval
- [ ] **T13.4** Write test: `fetch(PDO::FETCH_ASSOC)`, `fetchAll()`, `fetchColumn()`

### T14: SQLSTATE error mapping (~120 LOC)
- [ ] **T14.1** Create `pdo_fbird/pdo_fbird_error.c` with GDS-to-SQLSTATE mapping table
- [ ] **T14.2** Implement `pdo_fbird_fetch_error_func()` for `errorCode()` / `errorInfo()`
- [ ] **T14.3** Write test: error handling with `PDO::ERRMODE_EXCEPTION`

### T15: Custom attributes (~80 LOC)
- [ ] **T15.1** Register `PDO::FBIRD_ATTR_DIALECT`, `PDO::FBIRD_ATTR_CHARSET`,
  `PDO::FBIRD_ATTR_ROLE`, `PDO::FBIRD_ATTR_PAGE_BUFFERS` constants
- [ ] **T15.2** Implement `getAttribute()` / `setAttribute()` for custom attributes
- [ ] **T15.3** Write test: set/get custom attributes

### T16: BLOB/LOB support (~120 LOC)
- [ ] **T16.1** Implement `pdo_fbird_stmt_get_col()` BLOB handling via PDO LOB streams
- [ ] **T16.2** Implement `bindParam()` with `PDO::PARAM_LOB` support
- [ ] **T16.3** Write test: insert and read BLOBs via PDO

### T17: Comprehensive test suite (~200 LOC)
- [ ] **T17.1** `pdo_fbird/tests/pdo_fbird_connect.phpt` — connection variants
- [ ] **T17.2** `pdo_fbird/tests/pdo_fbird_query.phpt` — direct query execution
- [ ] **T17.3** `pdo_fbird/tests/pdo_fbird_prepared.phpt` — prepared statements
- [ ] **T17.4** `pdo_fbird/tests/pdo_fbird_transaction.phpt` — transaction lifecycle
- [ ] **T17.5** `pdo_fbird/tests/pdo_fbird_error.phpt` — error handling
- [ ] **T17.6** `pdo_fbird/tests/pdo_fbird_blob.phpt` — BLOB operations

### T18: Stubs, docs, CI (~80 LOC)
- [ ] **T18.1** Update `stubs/` with PDO driver class stubs
- [ ] **T18.2** Update `docker/docker-compose.yml` to build `pdo_fbird.so` in dev containers
- [ ] **T18.3** Update CI matrix to test PDO driver
- [ ] **T18.4** Run: full 12-target matrix + ASan + Valgrind (Validation Gate Part 2)

---

## Validation Gates

### Gate 1 (after Part 1)
```bash
bash scripts/test_matrix.sh                          # 12/12 green
docker compose exec php83-asan bash scripts/analysis/sanitizers.sh asan  # 3/3 PASS
docker compose run php82-fb3-dev bash scripts/run-valgrind.sh --quick    # 0 lost
grep -rn 'fb_safe_handle' --include="*.h" --include="*.c"               # 0 results
```

### Gate 2 (after Part 2)
```bash
bash scripts/test_matrix.sh                          # 12/12 green (incl. PDO tests)
docker compose exec php83-asan bash scripts/analysis/sanitizers.sh asan  # PASS
docker compose run php82-fb3-dev bash scripts/run-valgrind.sh --quick    # 0 lost
# PDO smoke test
php -d extension=firebird.so -d extension=pdo_fbird.so -r \
  "\$p = new PDO('fbird:host=localhost;dbname=/firebird/data/test.fdb', 'SYSDBA', 'masterkey'); echo \$p->getAttribute(PDO::ATTR_SERVER_VERSION);"
```

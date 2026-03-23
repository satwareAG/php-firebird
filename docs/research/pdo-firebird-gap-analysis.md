# PDO Firebird Gap Analysis

**Project**: satwareAG/php-firebird v8.0.1 (branch satware-main)
**Compared to**: php-src PDO Firebird driver (ext/pdo_firebird)
**Date**: 2026-03-23

## A. Executive Summary

The `pdo_fbird` driver implements a functional but incomplete PDO vtable: 10 of 17 dbh slots and 11 of 12 stmt slots are implemented, but critical gaps exist in quoting (NULL), transaction isolation (no level support), named parameters (not preprocessed), and Firebird 4+ type coercion (missing entirely). Test coverage is minimal (3 tests vs 46 in php-src). Closing these gaps would make `pdo_fbird` a full drop-in replacement with modern OO API advantages.

## B. PDO Driver Feature Comparison Matrix

| Slot | This Project | php-src | Notes |
|------|-------------|---------|-------|
| `closer` | impl | impl | - |
| `preparer` | impl | impl | - |
| `doer` | impl | impl | - |
| `quoter` | **NULL** | impl | Doubles quotes, wraps in single quotes |
| `begin` | impl (fbc wrapper) | impl (isolation levels) | This project: no isolation param |
| `commit` | impl | impl | - |
| `rollback` | impl | impl | - |
| `set_attribute` | partial (3 attrs) | full (9 attrs) | Missing: FETCH_TABLE_NAMES, date/time format, TRANSACTION_ISOLATION_LEVEL, WRITABLE_TRANSACTION |
| `last_id` | NULL | NULL | Neither supports (Firebird uses generators) |
| `fetch_error_func` | impl | impl | - |
| `get_attribute` | partial (5 attrs) | full (10 attrs) | Missing: FETCH_TABLE_NAMES, date/time format, TRANSACTION_ISOLATION_LEVEL, WRITABLE_TRANSACTION |
| `check_liveness` | **NULL** | impl (fb_ping) | FB 4+ only |
| `get_driver_methods` | NULL | NULL | - |
| `request_shutdown` | NULL | NULL | - |
| `in_manually_transaction` | **NULL** | impl | Tracks manual txn state |
| `get_gc` | NULL | NULL | - |
| `scanner` | NULL | NULL | - |

### Attribute Detail

| Attribute | This Project | php-src |
|-----------|-------------|---------|
| `AUTOCOMMIT` | set/get | set/get |
| `CONNECTION_STATUS` | get | get |
| `CLIENT_VERSION` | get | get |
| `SERVER_VERSION` | get | get |
| `SERVER_INFO` | get | get |
| `FETCH_TABLE_NAMES` | **missing** | set/get |
| `ATTR_DATE_FORMAT` | **missing** | set/get |
| `ATTR_TIME_FORMAT` | **missing** | set/get |
| `ATTR_TIMESTAMP_FORMAT` | **missing** | set/get |
| `TRANSACTION_ISOLATION_LEVEL` | **missing** | set/get (READ_COMMITTED/REPEATABLE_READ/SERIALIZABLE) |
| `WRITABLE_TRANSACTION` | **missing** | set/get |

## C. PDO Statement Feature Comparison Matrix

| Feature | This Project | php-src | Notes |
|---------|-------------|---------|-------|
| `dtor` | impl | impl | - |
| `execute` | impl | impl | - |
| `fetch` | impl | impl | - |
| `describe_col` | impl | impl | This project: no table name support |
| `get_col` | impl | impl | - |
| `param_hook` | impl | impl | - |
| `set_attr` | impl (CURSOR_FWDONLY) | impl (full) | Missing: CURSOR_SCROLL |
| `get_attr` | impl (CURSOR_FWDONLY) | impl (full) | - |
| `col_meta` | impl | impl | This project: no table name in meta |
| `next_rowset` | **NULL** | impl | Multi-statement support |
| `cursor_closer` | impl | impl | - |
| Named params (`:name`) | **no preprocessing** | `php_firebird_preprocess()` | Positional only via `supports_placeholders` |
| Blob streaming (bindColumn) | **no** | impl (FB_BLOB_ID detection) | - |
| Scrollable cursors | **no** | impl (ABS/REL/FIRST/LAST/PRIOR) | Uses `isc_dsql_set_cursor_name` + `isc_dsql_fetch` |
| EXECUTE BLOCK named params | **no** | impl | - |
| FB 4+ type coercion | **no** | impl | INT128, DECFLOAT, TIME_TZ, TIMESTAMP_TZ |
| Nullable params for NOT NULL cols | **no** | impl (`sqltype \|= 1`) | Allows NULL binding to NOT NULL columns |

## D. Firebird Client Version Compatibility Matrix

| Feature | FB 3.x | FB 4.x | FB 5.x | This Project | php-src |
|---------|--------|--------|--------|-------------|---------|
| `fb_ping()` / check_liveness | no | yes | yes | no | yes |
| INT128 type | no | yes | yes | fbird_* (raw) | PDO coercion |
| DECFLOAT (16/34) | no | yes | yes | fbird_* (raw) | PDO coercion |
| TIME_TZ type | no | yes | yes | no | PDO coercion or native (client>=4) |
| TIMESTAMP_TZ type | no | yes | yes | no | PDO coercion or native (client>=4) |
| `isc_dpb_set_bind` | no | yes | yes | fbc_set_bind | no |
| `isc_get_client_version()` | yes | yes | yes | fbc_get_client_version | yes |
| Batch DML | no | yes | yes | fbird_batch_* | no |
| `fb_set_bind` / `fb_get_bind` | no | yes | yes | fbc_set_bind/get_bind | no |
| ATTR_DATE/TIME/TIMESTAMP_FORMAT | no | no | no | no | PHP 8.4+ constants |
| TRANSACTION_ISOLATION_LEVEL | any | any | any | no | PHP 8.4+ constants |

## E. Missing ISC/FB API Functions - Expansion Opportunities

### Already Wrapped in firebird_utils.h (fbu_/fbc_/fbt_/fbs_/fbb_/fbe_/fbsvc_/fba_/fbm_/fbxpb_)

| Prefix | Count | Scope |
|--------|-------|-------|
| `fbu_*` | 13 | Date/time encode/decode, sqlcode, client version, field info |
| `fbc_*` | 9 | Connect, disconnect, drop, create, info, attachment, server version |
| `fbt_*` | 11 | Start, commit, rollback, retain, limbo, reconnect, info |
| `fbs_*` | 16 | Prepare, execute, cursor, fetch, metadata, type, affected rows |
| `fbb_*` | 11 | Create/open/put/get/close/seek/cancel/info blob |
| `fbe_*` | 10 | Queue/cancel/wait/decode events, event block building |
| `fbsvc_*` | 6 | Attach/detach/start/query service manager |
| `fba_*` | 3 | Array lookup bounds, get/put slice |
| `fbm_*` | 14 | Metadata interop (offset, type, subtype, length, scale, charset, field, alias, **relation**) |
| `fbxpb_*` | 2 | TPB builder, free |
| `fbbatch_*` | 12 | FB 4.0+ batch DML (create/add/execute/close/blob handling/errors) |

**Key observation**: `fbm_get_relation()` is already wrapped. FETCH_TABLE_NAMES in PDO is feasible without new C wrappers.

### Not Wrapped in firebird_utils.h - FB 3.0+ Client

| Function | Purpose | Impact |
|----------|---------|--------|
| `isc_vax_integer` | Decode VAX integer from status vector | Low (legacy) |
| `isc_ftof` | Format float to string | Low |
| `isc_blob_default_desc` | Get default BLOB description | Low |
| `isc_blob_gen_bpb` | Generate BLOB parameter block | Medium (dynamic BPB construction) |
| `isc_array_gen_sdl` | Generate array SDL | Low (array fields niche) |
| `isc_dsql_fetch` (scroll) | Fetch with orientation | High (scrollable cursors need this, not just `fbs_fetch`) |
| `fb_cancel_operation` | Cancel running statement | Medium (async cancel) |

### Not Wrapped in firebird_utils.h - FB 4.0+ Only

| Function | Purpose | Impact |
|----------|---------|--------|
| `fb_ping` | Connection liveness check | High (PDO check_liveness needs this) |
| `fb_set_bind` | Set bind config (decfloat sep, timezone) | Medium (decimal formatting) |
| `fb_get_bind` | Get bind config | Medium |

### Available via fbc_* but Not Exposed as PHP fbird_* Functions

| Wrapper | Purpose | PHP Exposure |
|---------|---------|-------------|
| `fba_lookup_bounds` / `fba_get_slice` / `fba_put_slice` | Array field operations | Exposed via OO `Firebird\Connection` only |
| `fbsvc_start` | Start service action (backup/restore) | Exposed via OO `Firebird\Service` only |
| `fbs_set_cursor_name` | Set cursor name for WHERE CURRENT OF | Internal only, no PHP function |
| `fbu_decode_time_tz` / `fbu_decode_timestamp_tz` | FB 4+ timezone types | Exposed via OO only |
| `fbu_encode_time_tz` / `fbu_encode_timestamp_tz` | FB 4+ timezone types | Exposed via OO only |

## F. Test Coverage Gap Analysis

### Existing Tests (3)

| Test | Coverage |
|------|----------|
| `pdo_fbird_001.phpt` | Basic connect + query |
| `pdo_fbird_002.phpt` | Prepare + execute |
| `pdo_fbird_003.phpt` | Transaction commit/rollback |

### Missing Test Scenarios (from php-src 46 tests)

| php-src Test | Scenario | Priority |
|-------------|----------|----------|
| `attr_datetime_format.phpt` | Date/time/timestamp format attributes | P1 |
| `autocommit.phpt` | Autocommit mode behavior | P0 |
| `autocommit_change_mode.phpt` | Switch autocommit mid-connection | P0 |
| `bug_15604.phpt` | Transaction error recovery | P0 |
| `bug_47415.phpt` | Prepare error handling | P0 |
| `bug_48877.phpt` | NULL parameter binding | P0 |
| `bug_53280.phpt` | Quote handling edge case | P0 |
| `bug_62024.phpt` | DDL in transaction | P1 |
| `bug_64037.phpt` | Column metadata | P1 |
| `bug_72583.phpt` | Multiple statement execution | P1 |
| `bug_72931.phpt` | Named parameter resolution | P0 |
| `bug_73087.phpt` | Fetch mode variations | P1 |
| `bug_74462.phpt` | BLOB handling | P1 |
| `bug_76448.phpt` | Error code mapping | P0 |
| `bug_76449.phpt` | Error code mapping (2) | P0 |
| `bug_76450.phpt` | Error code mapping (3) | P0 |
| `bug_76452.phpt` | Connection error handling | P0 |
| `bug_76488.phpt` | Statement cleanup | P1 |
| `bug_77863.phpt` | Parameter type coercion | P0 |
| `bug_80521.phpt` | Dialect handling | P1 |
| `bug_aaa.phpt` | Test infrastructure | P2 |
| `connect.phpt` | Connection variants | P0 |
| `ddl.phpt` / `ddl2.phpt` | DDL operations | P1 |
| `dialect_1.phpt` | Dialect 1 SQL | P1 |
| `error_handle.phpt` | Error handling patterns | P0 |
| `execute.phpt` | Execute variations | P0 |
| `execute_block.phpt` | EXECUTE BLOCK with named params | P0 |
| `fb4_datatypes.phpt` | FB 4+ type output | P1 |
| `fb4_datatypes_params.phpt` | FB 4+ type input params | P1 |
| `get_api_version.phpt` | API version constant | P2 |
| `gh10908.phpt` | GitHub bug regression | P1 |
| `gh13119.phpt` | GitHub bug regression | P1 |
| `gh17383.phpt` | GitHub bug regression | P1 |
| `gh18276.phpt` | GitHub bug regression | P1 |
| `gh8576.phpt` | GitHub bug regression | P1 |
| `ignore_parammarks.phpt` | Placeholder parsing edge | P0 |
| `payload_test.phpt` | Large payload handling | P1 |
| `pdofirebird_001.phpt` | (duplicate of basic) | P2 |
| `pdofirebird_002.phpt` | (duplicate) | P2 |
| `persistent_connect.phpt` | Persistent connections | P1 |
| `php_8.5_deprecations.phpt` | Deprecation warnings | P2 |
| `rowCount.phpt` | rowCount() accuracy | P0 |
| `setCursorAttribute.phpt` | Cursor attributes | P1 |
| `transaction_access_mode.phpt` | Writable transaction | P1 |
| `transaction_isolation_level_attr.phpt` | Isolation level attribute | P0 |
| `transaction_isolation_level_behavior.phpt` | Isolation level behavior | P0 |

**Summary**: 0 of 46 php-src scenarios covered. 14 rated P0, 15 rated P1, 5 rated P2.

## G. Prioritized Implementation Roadmap

### P0 - Critical (Drop-in Replacement Blockers)

| Item | Description | Complexity | FB Version | Stubs | Tests |
|------|-------------|-----------|------------|-------|-------|
| Quoter | Implement `pdo_fbird_handle_quoter` - double single quotes, wrap in single quotes | S | any | No (internal) | 2-3 |
| Transaction isolation | Support READ_COMMITTED/REPEATABLE_READ/SERIALIZABLE in begin + set/get_attribute | M | any | No (PHP 8.4 consts) | 3-4 |
| Proper autocommit | Use read_committed+rec_version+retain; add `in_manually_transaction` tracking | M | any | No | 3 |
| Named parameters | Port `php_firebird_preprocess()` for :name resolution in INSERT/UPDATE/DELETE/SELECT/EXECUTE BLOCK | M | any | No (internal) | 4-5 |
| Nullable param binding | Set `sqltype \|= 1` to allow NULL for NOT NULL columns | S | any | No | 1-2 |

### P1 - Important (Feature Parity)

| Item | Description | Complexity | FB Version | Stubs | Tests |
|------|-------------|-----------|------------|-------|-------|
| FB 4+ type coercion | INT128->VARCHAR(46), DEC16->VARCHAR(24), DEC34->VARCHAR(43), TIME_TZ/TIMESTAMP_TZ | M | 4.0+ | No (internal) | 4-5 |
| FETCH_TABLE_NAMES | Add table name to describe_col + col_meta; store in handle struct | S | any | No | 2-3 |
| Date/time format attrs | ATTR_DATE_FORMAT, ATTR_TIME_FORMAT, ATTR_TIMESTAMP_FORMAT set/get | S | any | No (PHP 8.4 consts) | 3 |
| Scrollable cursors | FETCH_ORI_ABS/REL/FIRST/LAST/PRIOR via isc_dsql_set_cursor_name | L | any | No | 5-6 |
| WRITABLE_TRANSACTION | set/get_attribute for read/write txn mode | S | any | No (PHP 8.4 const) | 2 |
| Blob streaming via bindColumn | Detect FB_BLOB_ID, create blob handle for stream reading | M | any | No | 2-3 |
| next_rowset | Check isc_info_sql_stmt_type for multi-statement | M | any | No | 1-2 |

### P2 - Nice-to-Have (Differentiation)

| Item | Description | Complexity | FB Version | Stubs | Tests |
|------|-------------|-----------|------------|-------|-------|
| check_liveness | Wrap `fb_ping()` (new C wrapper needed, FB 4+ only, graceful degradation) | S | 4.0+ | No | 1-2 |
| Service API via PDO | Expose backup/restore/user management through PDO attributes | L | any | No | 3-4 |
| Array field support | Expose `isc_array_get_slice`/`put_slice` through PDO | L | any | Yes (new fbird_*) | 4-5 |
| Async event polling | Expose `fbc_event_*` through PDO attributes | L | any | Yes (new fbird_*) | 3-4 |
| Bind config (fb_set_bind) | Expose decimal separator, datetime format via PDO attribute | S | 4.0+ | No | 1-2 |

## H. Strategic Positioning

### Differentiation from php-src Pdo\Firebird

| Dimension | php-src Pdo\Firebird | This Project (pdo_fbird) |
|-----------|---------------------|-------------------------|
| **Client API** | Legacy `isc_*` macros | Modern OO C++ wrappers (`fbc_*`) |
| **Min client version** | FB 2.5 | FB 3.0 (cleaner API surface) |
| **Batch DML** | Not available | `fbird_batch_*` already in procedural API |
| **Service API** | Not available | `Firebird\Service` class exists |
| **DSN prefix** | `firebird:` (conflicts if both loaded) | `fbird:` (coexists) |
| **PHP version support** | Ships with PHP (may deprecate) | Independent release cadence |
| **FB 4+ features** | Partial (type coercion only) | Bind config, batch DML, ping |
| **Test suite** | 46 tests | 3 tests (gap to close) |

### Recommended Positioning Statement

`pdo_fbird` targets modern Firebird deployments (FB 3.0+ client, FB 4.0+ server preferred) where the legacy `isc_*` API is a liability. Once P0 gaps are closed, it becomes a strict superset of php-src's driver with exclusive features (batch DML, service API, bind configuration). The independent release model allows shipping fixes and FB 4+/5+ features faster than PHP's release cycle.

### Risks

| Risk | Mitigation |
|------|-----------|
| php-src deprecating ext/pdo_firebird | Become the default recommendation; ensure P0 parity first |
| FB 3.0 client minimum | Document clearly; FB 2.5 servers work via FB 3+ client |
| Named param preprocessing complexity | Port proven algorithm from php-src; add regression tests |
| FB 4+ features on older servers | Runtime version detection; graceful degradation |
</task_progress>
- [x] Read firebird_utils.h for fbc_* API inventory
- [x] Read docs/pdo-driver.md
- [x] Read docs/oop-api.md
- [x] List pdo_fbird/tests/ for existing test inventory
- [x] Write docs/research/pdo-firebird-gap-analysis.md</task_progress>
</write_to_file>
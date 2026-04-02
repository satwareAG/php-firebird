# Implementation Plan

[Overview]
Modernize php-firebird extension API for v11.0 with typed arginfo, call_user_function elimination, and resource-to-object migration.

This plan covers the three remaining v11.0 modernization items (M2, M3, M12) tracked in spec-v11-modernization.md and GitHub issues #175-#177. The implementation order is designed to minimize risk: M2 (typed arginfo) is purely internal and non-breaking, M12 (direct C calls) is an internal refactor of the OOP layer, and M3 (resource-to-object) is the breaking change that affects public API and tests.

Current state: 80 of 82 arginfo declarations use untyped `ZEND_BEGIN_ARG_INFO_EX`/`ZEND_BEGIN_ARG_INFO` macros. The OOP layer (fbird_classes.c) uses `call_user_function()` in 6 places to delegate to procedural functions. All procedural functions return PHP resources via `zend_register_resource()`. Firebird\* OOP classes already exist but wrap resource pointers internally.

[Types]
No new PHP-visible types are introduced; existing Firebird\* class objects replace resource returns.

### Resource Type Inventory (current)

| Resource Type | Variable | Registered In | Struct |
|---|---|---|---|
| `le_link` | `IBG(le_link)` | firebird.c MINIT | `fbird_db_link` |
| `le_plink` | `IBG(le_plink)` | firebird.c MINIT | `fbird_db_link` |
| `le_trans` | `IBG(le_trans)` | firebird.c MINIT | `fbird_trans` |
| `le_result` | `IBG(le_result)` | firebird.c MINIT | `fbird_result` |
| `le_blob` | `IBG(le_blob)` | firebird.c MINIT | `fbird_blob` |
| `le_service` | fbird_service.c static | fbird_service.c | `fbird_service` |
| `le_event` | fbird_events.c static | fbird_events.c | `fbird_event` |

### Existing OOP Classes (C structs in fbird_classes.c)

| Class | Internal Struct | Current Wrapping |
|---|---|---|
| `Firebird\Connection` | `fbird_connection_obj { zend_resource *conn_res; zend_object std; }` | Wraps le_link resource |
| `Firebird\Transaction` | `fbird_transaction_obj { void *fbt_trans; zend_object std; }` | Direct OO API pointer |
| `Firebird\Statement` | `fbird_statement_obj { zend_resource *query_res; zend_object std; }` | Wraps le_result resource |
| `Firebird\ResultSet` | `fbird_resultset_obj { zend_resource *query_res; zend_object std; }` | Wraps le_result resource |
| `Firebird\Blob` | `fbird_blob_obj { ... }` | TBD |
| `Firebird\Service` | `fbird_service_obj { ... }` | TBD |
| `Firebird\EventPoller` | `fbird_event_obj { ... }` | TBD |

### Arginfo Return Type Mapping (from stubs)

| Function Family | Return Type | Macro |
|---|---|---|
| `fbird_connect`, `fbird_pconnect` | `Firebird\Connection\|false` | `ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX(..., fbird_connection_ce, MAY_BE_FALSE)` |
| `fbird_trans` | `Firebird\Transaction\|false` | OBJ_TYPE_MASK_EX |
| `fbird_query`, `fbird_prepare` | `Firebird\Statement\|false` | OBJ_TYPE_MASK_EX |
| `fbird_execute` | `Firebird\ResultSet\|false` | OBJ_TYPE_MASK_EX |
| `fbird_fetch_row`, `fbird_fetch_assoc`, `fbird_fetch_object` | `array\|object\|false` | TYPE_MASK_EX |
| `fbird_blob_create`, `fbird_blob_open` | `Firebird\Blob\|false` | OBJ_TYPE_MASK_EX |
| `fbird_affected_rows` | `int` | TYPE_INFO_EX(IS_LONG) |
| `fbird_num_fields`, `fbird_num_params` | `int` | TYPE_INFO_EX(IS_LONG) |
| `fbird_errmsg` | `string\|false` | TYPE_MASK_EX(MAY_BE_STRING\|MAY_BE_FALSE) |
| `fbird_errcode` | `int\|false` | TYPE_MASK_EX(MAY_BE_LONG\|MAY_BE_FALSE) |
| `fbird_close`, `fbird_commit`, `fbird_rollback` | `bool` | TYPE_INFO_EX(_IS_BOOL) |
| `fbird_escape_string` | `string` | Already migrated |
| Batch functions | various | Per-function |
| Service/backup/restore | various | Per-function |

[Files]
All changes are in existing files; no new source files needed. New test files for migration validation.

### Modified Files

| File | Changes | Phase |
|---|---|---|
| `firebird.c` | Rewrite 80 arginfo declarations to typed return macros | M2 |
| `fbird_classes.c` | Replace 6 call_user_function sites with direct C calls; update internal struct wrapping for M3 | M12, M3 |
| `fbird_classes.h` | Update struct definitions, add accessor functions | M3 |
| `fbird_connection.c` | Change `zend_register_resource` returns to object returns | M3 |
| `fbird_transaction.c` | Change resource returns to object returns | M3 |
| `fbird_query_exec.c` | Change resource returns to object returns | M3 |
| `fbird_query_prepare.c` | Change resource returns to object returns | M3 |
| `fbird_blobs.c` | Change resource returns to object returns | M3 |
| `fbird_service.c` | Change resource returns to object returns | M3 |
| `fbird_events.c` | Change resource returns to object returns | M3 |
| `fbird_result.c` | Update resource fetching to object extraction | M3 |
| `fbird_metadata.c` | Update resource fetching to object extraction | M3 |
| `fbird_inspection.c` | Update resource fetching to object extraction | M3 |
| `fbird_query_bind.c` | Update resource fetching to object extraction | M3 |
| `fbird_query_array.c` | Update resource fetching to object extraction | M3 |
| `fbird_batch.c` | Update resource fetching to object extraction | M3 |
| `php_fbird_includes.h` | Add object extraction macros, update resource type usage | M3 |
| `stubs/firebird-stubs.php` | Update return types from `resource` to `Firebird\*` classes | M2/M3 |
| `stubs/firebird-classes.php` | Verify class method signatures | M3 |
| `phpstan/fbird.stub.php` | Update return types | M2/M3 |
| `config.w32` | No changes needed | - |
| `config.m4` | No changes needed | - |

### New Test Files

| File | Purpose | Phase |
|---|---|---|
| `tests/v11_typed_arginfo_reflection.phpt` | Verify ReflectionFunction shows typed returns | M2 |
| `tests/v11_resource_bc_break.phpt` | Verify instanceof works, is_resource() returns false | M3 |
| `tests/v11_object_returns.phpt` | Verify object type returns from procedural functions | M3 |

### Modified Test Files (21 files with is_resource)

All 17 files using bare `is_resource()` must be updated to use `instanceof` checks:
- `tests/coverage/batch_error_paths.phpt`
- `tests/coverage/batch_limits.phpt`
- `tests/coverage/events_error_handling.phpt`
- `tests/coverage/execute_procedure_returning.phpt`
- `tests/coverage/execute_statement_query.phpt`
- `tests/coverage/inspection_deep.phpt`
- `tests/coverage/query_params_tx.phpt`
- `tests/coverage/transaction_coverage.phpt`
- `tests/execute_safety_001.phpt`
- `tests/fbird_batch_no_params_001.phpt`
- `tests/fbird_service_oo_001.phpt`
- `tests/issue120.phpt`
- `tests/issue131.phpt`
- `tests/pdo_fbird_blob_handling.phpt`
- `tests/pdo_fbird_blob_stream.phpt`
- `tests/test_blob_stream.phpt`
- 4 hybrid files need `is_resource()` branch removed

[Functions]
80 arginfo declarations rewritten; 6 call_user_function calls replaced with direct C calls; ~30 functions updated to return objects.

### M2: Arginfo Rewrite (firebird.c only)

Every `ZEND_BEGIN_ARG_INFO_EX` and `ZEND_BEGIN_ARG_INFO` becomes one of:
- `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(name, ref, req, type, nullable)` - for single types (IS_STRING, IS_LONG, _IS_BOOL)
- `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(name, ref, req, mask)` - for union types (MAY_BE_STRING|MAY_BE_FALSE)
- `ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX(name, ref, req, class, mask)` - for object|false returns (Phase M3 only)

**Phase M2 strategy**: Use TYPE_INFO_EX and TYPE_MASK_EX only. Do NOT use OBJ return types yet (that's M3). Return types for resource-returning functions use `MAY_BE_OBJECT|MAY_BE_FALSE` as a transitional step.

### M12: call_user_function Elimination (fbird_classes.c)

| Line | Current Call | Replacement |
|---|---|---|
| 114 | `call_user_function(NULL, NULL, &fn, &retval, 7, args)` calling "fbird_connect" | Direct call to `_php_fbird_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU)` or inline equivalent |
| 407 | `fbird_call_fn("fbird_fetch_assoc", ...)` | Direct call to `_php_fbird_fetch_hash(INTERNAL_FUNCTION_PARAM_PASSTHRU, FETCH_ASSOC)` |
| 454 | `fbird_call_fn("fbird_execute", ...)` | Direct call to `_php_fbird_exec(INTERNAL_FUNCTION_PARAM_PASSTHRU)` |
| 525 | `fbird_call_fn("fbird_trans", ...)` | Direct call to `_php_fbird_trans(INTERNAL_FUNCTION_PARAM_PASSTHRU)` |
| 541 | `fbird_call_fn("fbird_prepare", ...)` | Direct call to `_php_fbird_alloc_query(...)` |
| 388 | `fbird_call_fn()` wrapper definition | Remove entirely after all call sites migrated |

### M3: Resource-to-Object Conversion Functions

Functions that currently call `zend_register_resource()` and return resources:
- `_php_fbird_connect()` in fbird_connection.c - returns le_link/le_plink
- `PHP_FUNCTION(fbird_trans)` in fbird_transaction.c - returns le_trans
- `PHP_FUNCTION(fbird_query)` in fbird_query_exec.c - returns le_result
- `PHP_FUNCTION(fbird_prepare)` in fbird_query_prepare.c - returns le_result
- `PHP_FUNCTION(fbird_execute)` in fbird_query_exec.c - returns le_result
- `PHP_FUNCTION(fbird_blob_create)` in fbird_blobs.c - returns le_blob
- `PHP_FUNCTION(fbird_blob_open)` in fbird_blobs.c - returns le_blob
- `PHP_FUNCTION(fbird_service_attach)` in fbird_service.c - returns le_service
- `PHP_FUNCTION(fbird_set_event_handler)` in fbird_events.c - returns le_event

Functions that consume resources via `zend_fetch_resource()`/`zend_fetch_resource2()` - approximately 40+ call sites across all fbird_*.c files.

[Classes]
No new classes needed; existing Firebird\* classes become the return type of procedural functions.

### Modified Classes

| Class | Change | Details |
|---|---|---|
| `Firebird\Connection` | Struct becomes primary, not resource wrapper | `fbird_connection_obj` embeds `fbird_db_link` directly instead of `zend_resource *conn_res` |
| `Firebird\Transaction` | Already uses direct OO pointer | Minimal changes |
| `Firebird\Statement` | Embeds query data directly | `fbird_statement_obj` embeds `fbird_result` instead of `zend_resource *query_res` |
| `Firebird\ResultSet` | Embeds query data directly | Same pattern |
| `Firebird\Blob` | Embeds blob data directly | TBD based on struct analysis |
| `Firebird\Service` | Embeds service data directly | TBD |

### Object Extraction Macro (new, php_fbird_includes.h)

```c
/* Extract typed object from zval (replaces zend_fetch_resource) */
#define FBIRD_FETCH_CONNECTION(zv) \
    ((fbird_connection_obj *)((char *)Z_OBJ_P(zv) - XtOffsetOf(fbird_connection_obj, std)))

#define FBIRD_FETCH_TRANSACTION(zv) \
    ((fbird_transaction_obj *)((char *)Z_OBJ_P(zv) - XtOffsetOf(fbird_transaction_obj, std)))
```

[Dependencies]
No new external dependencies; only internal Zend API macro changes.

The migration uses standard Zend API macros available since PHP 8.0:
- `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX` (PHP 7.0+)
- `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX` (PHP 8.0+)
- `ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX` (PHP 8.0+)
- Object custom handlers: `create_object`, `free_obj`, `get_gc` (PHP 8.0+)

VERSION file bump: 10.6.1 -> 11.0.0 (after M3, breaking change)

[Testing]
277 existing .phpt tests; 21 files need is_resource updates; 3 new test files for migration validation.

### Phase M2 Testing
- Run full test suite - arginfo changes are internal, no test changes needed
- New test: `v11_typed_arginfo_reflection.phpt` verifying `ReflectionFunction::getReturnType()` for key functions
- Run `scripts/check-stubs-sync.sh` to verify stub consistency

### Phase M12 Testing
- Run full test suite - internal refactor should be transparent
- Verify OOP tests still pass: `tests/fbird_service_oo_001.phpt`, `tests/event_poller_wrapper.phpt`, `tests/issue120.phpt`

### Phase M3 Testing
- Update 21 test files replacing `is_resource($x)` with `$x instanceof Firebird\Connection` (etc.)
- Remove `get_resource_type()` calls
- Update `var_dump` expected output from `resource(N) of type (...)` to `object(Firebird\Connection)#N (...)`
- New test: `v11_resource_bc_break.phpt` confirming `is_resource()` returns false
- New test: `v11_object_returns.phpt` verifying all procedural functions return objects
- Run ASAN + Valgrind for memory safety

[Implementation Order]
Three phases in strict sequence: M2 (non-breaking) -> M12 (internal refactor) -> M3 (breaking change).

### Phase 1: M2 - Typed Arginfo (issue #175)
Branch: `feat/175-typed-arginfo`

1. Rewrite all 80 untyped arginfo declarations in firebird.c to typed macros
   - Group 1 (7): Zero-param functions (fbird_errmsg, fbird_errcode, etc.)
   - Group 2 (8): Connection functions
   - Group 3 (6): Transaction functions
   - Group 4 (12): Query/execute functions
   - Group 5 (8): Result/fetch functions
   - Group 6 (10): Blob functions
   - Group 7 (6): Service functions
   - Group 8 (14): Batch functions (FB_API_VER >= 40 guarded)
   - Group 9 (8): Metadata/inspection functions
   - Group 10 (1): Remaining (escape_string already done)
2. Update stubs to match new return types
3. Run `scripts/check-stubs-sync.sh`
4. Add reflection test
5. Full CI run
6. Create PR, merge to satware-main

### Phase 2: M12 - Direct C Calls (issue #177)
Branch: `feat/177-direct-c-calls`

1. Identify internal C function signatures for all 6 call sites
2. Replace FirebirdConnection::__construct call_user_function with direct `_php_fbird_connect`
3. Replace FirebirdResultSet::fetch with direct `_php_fbird_fetch_hash`
4. Replace FirebirdStatement::execute with direct `_php_fbird_exec`
5. Replace FirebirdConnection::prepare trans+prepare with direct calls
6. Remove `fbird_call_fn()` helper
7. Full CI run + ASAN
8. Create PR, merge to satware-main

### Phase 3: M3 - Resource-to-Object (issue #176)
Branch: `feat/176-resource-to-object`

1. Refactor `fbird_connection_obj` to embed `fbird_db_link` data directly
2. Update `_php_fbird_connect()` to return Firebird\Connection object
3. Add `FBIRD_FETCH_CONNECTION()` macro and update all consumer sites
4. Repeat for Transaction, Statement/ResultSet, Blob, Service, Event
5. Remove `zend_register_resource()` calls (keep persistent link support via spl_object_hash)
6. Update all 21 test files with is_resource -> instanceof
7. Add migration test files
8. Update stubs for object return types
9. Bump VERSION to 11.0.0
10. Write migration guide (docs/MIGRATION_V11.md)
11. Full CI + ASAN + Valgrind
12. Create PR, merge to satware-main, tag v11.0.0

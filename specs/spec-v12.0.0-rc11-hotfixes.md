---
description: >-
  Hotfixes for v12.0.0-rc.11 addressing 5 issues found during
  doctrine-firebird-driver integration testing against v12.0.0-rc.10,
  plus 2 additional findings from code review.
tags: [hotfix, v12.0.0, rc.11, arginfo, throw-mode, batchhandle, null-deref]
priority: 1
---

> **Status: ACTIVE** - 5 issues from downstream integration testing + 2 code review findings

# Spec: v12.0.0-rc.11 Hotfixes

## Goal

Fix 5 issues (#305-#309) discovered during doctrine-firebird-driver
integration against v12.0.0-rc.10, plus 2 additional findings from
code review. These are pre-release bug fixes folded into the v12.0.0
release cycle before the stable tag.

## Version

`v12.0.0-rc.11` - branch `release/v12.0.0`

## Priority

**P1** - blocks stable v12.0.0 release

## Success Criteria

### HF-1: fbird_get_client_version return type (#308)

The C implementation returns `double` via `RETURN_DOUBLE`, but arginfo
declares `IS_STRING` and the stub declares `string`. ReflectionFunction
lies to consumers.

- [ ] `firebird.c` arginfo: `IS_STRING` -> `IS_DOUBLE`
- [ ] `stubs/firebird-stubs.php`: `string` -> `float`
- [ ] `phpstan/fbird.stub.php`: `string` -> `float` (if exists)
- [ ] `ReflectionFunction::getReturnType()` returns `float`
- [ ] Runtime `gettype()` returns `"double"`

### HF-2: Arginfo parameter types + NULL-deref (#307)

5 functions declare `IS_STRING` for their first parameter but accept
resource/object. 4 functions have nullable array mismatch: arginfo says
non-nullable `IS_ARRAY`, stubs say `?array`, C parse accepts null. If a
consumer passes `null` per the stubs, `Z_ARRVAL_P()` may dereference NULL
and segfault.

Additionally, `fbird_trans_start` has a reverse mismatch: arginfo says
nullable `IS_ARRAY,1` but C parser uses `"a"` (non-nullable).

#### HF-2a: IS_STRING parameter mismatches (5 functions)

- [ ] `fbird_execute` arginfo: `IS_STRING` -> `ZEND_ARG_INFO(0, query)` (mixed)
- [ ] `fbird_free_query` arginfo: `IS_STRING` -> `ZEND_ARG_INFO(0, query)` (mixed)
- [ ] `fbird_num_params` arginfo: `IS_STRING` -> `ZEND_ARG_INFO(0, query)` (mixed)
- [ ] `fbird_param_info` arginfo: `IS_STRING` -> `ZEND_ARG_INFO(0, query)` (mixed)
- [ ] `fbird_batch_create` arginfo: `IS_STRING` -> `ZEND_ARG_INFO(0, query)` (mixed)

#### HF-2b: Nullable array NULL-deref (4 functions)

- [ ] `fbird_execute_statement` arginfo: `IS_ARRAY,0` -> `IS_ARRAY,1` (nullable)
- [ ] `fbird_execute_query` arginfo: `IS_ARRAY,0` -> `IS_ARRAY,1` (nullable)
- [ ] `fbird_execute_auto` arginfo: `IS_ARRAY,0` -> `IS_ARRAY,1` (nullable)
- [ ] `fbird_query_params_tx` arginfo: `IS_ARRAY,0` -> `IS_ARRAY,1` (nullable)
- [ ] 4 `Z_ARRVAL_P(params_arg)` calls guarded by `Z_TYPE_P(params_arg) == IS_ARRAY`

#### HF-2c: fbird_trans_start reverse mismatch

- [ ] `fbird_transaction.c:299`: `"|za"` -> `"|za!"` (accept null options)
- [ ] Add `Z_TYPE_P` guard before `Z_ARRVAL_P` if options used

### HF-3: MAY_BE_RESOURCE -> MAY_BE_OBJECT (#306)

20+ functions declare `MAY_BE_RESOURCE` as return type, but runtime
returns `Firebird\*` objects. ReflectionFunction reports `resource|false`
when the runtime produces `Firebird\Statement|false` etc.

- [ ] All 20 `MAY_BE_RESOURCE` in arginfo replaced with `MAY_BE_OBJECT`
- [ ] `fbird_query` (L208): add `MAY_BE_LONG` (returns affected row count)
- [ ] `fbird_execute` (L247): add `MAY_BE_LONG` (returns affected row count)
- [ ] `ReflectionFunction::getReturnType()` shows `object` not `resource`
- [ ] Stubs already correct (no stub changes needed)

### HF-4: THROW mode bypass (#305)

~40 procedural functions use `php_error_docref(NULL, E_WARNING, ...)`
instead of `_php_fbird_module_error(...)` for failure paths. Under
`FBIRD_EXCEPTION_MODE_THROW`, these bypass the exception mechanism -
`FBG(sql_code)` stays 0, `FBG(errmsg)` stays empty, no exception thrown.

The `_php_fbird_module_error` helper already:
- Clears `last_status`
- Sets `FBG(sql_code) = -999`
- Formats `FBG(errmsg)`
- Respects `FBG(exception_mode)` (throws under THROW mode)

#### HF-4a: Replace php_error_docref with _php_fbird_module_error

- [ ] `fbird_query_prepare.c` (4 calls: L263, L267, L271, L276)
- [ ] `fbird_query_exec.c` (2 calls: L111, L116)
- [ ] `fbird_batch.c` (10 calls: L213, L602, L650, L683, L715, L721, L754, L783, L812, L841)
- [ ] `fbird_connection.c` (4 calls: L543, L625, L632, L743)
- [ ] `fbird_transaction.c` (9 calls: L131, L263, L323, L402, L408, L520, L642, L826, L958)
- [ ] `fbird_service.c` (1 call: L325)
- [ ] `fbird_events.c` (2 calls: L181, L297)
- [ ] `fbird_blobs.c` (1 call: L751)
- [ ] `fbird_error.c` (1 call: L135 - invalid exception mode)
- [ ] `firebird.c` (6 calls: L1014, L1019, L1107, L1113, L1118, L1239)

**EXCLUDE** (keep as-is):
- `fbird_query_exec.c:1058` - `E_DEPRECATED` (not a warning)
- `fbird_error.c:201, 225` - inside error infrastructure (SILENT-mode fallback)

#### HF-4b: Add _php_fbird_module_error to silent RETURN_FALSE paths

- [ ] `fbird_service.c:676-678` - invalid service handle in fbird_server_info
- [ ] `fbird_inspection.c:321-323` - attachment NULL in fbird_list_table_blockers
- [ ] `fbird_inspection.c:326-328` - transaction NULL in fbird_list_table_blockers
- [ ] `fbird_inspection.c:52-54` - attachment NULL in _fbird_exec_kill
- [ ] `fbird_inspection.c:57-59` - transaction NULL in _fbird_exec_kill

#### HF-4c: Verification

- [ ] THROW mode: exceptions thrown with errcode -999 and correct errmsg
- [ ] SILENT mode: warnings still emitted (backward compatible)
- [ ] `fbird_errcode()` / `fbird_errmsg()` return correct values after each failure

### HF-5: BatchHandle OOP methods (#309)

`Firebird\BatchHandle` is registered with `NULL` methods table. It's an
opaque marker class. The C implementations already exist as
`PHP_FUNCTION(fbird_batch_*)` and should be exposed as methods.

- [ ] `fbird_classes.c:102`: replace `NULL` with method entries table
- [ ] 6 methods wired to existing implementations:
  - `getBlobAlignment(): int|false`
  - `setDefaultBpb(string $bpb): bool`
  - `cancel(): bool`
  - `execute(): array|false`
  - `add(mixed ...$args): bool`
  - `addBlob(string $data, int $type = 0): string|false`
- [ ] Stub updated with method signatures
- [ ] `ReflectionClass` shows 6 public methods

### HF-6: Additional findings (code review)

#### HF-6a: fbird_free_query silent RETURN_FALSE

`_php_fbird_free_query_impl` at `fbird_query_exec.c:1544-1546` silently
returns FALSE when argument is not a resource and not a recognized object
type. Should emit `zend_type_error` for unrecognized types.

- [ ] Replace silent `RETURN_FALSE` with `zend_type_error(...)`

#### HF-6b: stubs_sync.phpt return type assertions

`tests/stubs_sync.phpt` currently only checks function existence, not
return type consistency between arginfo and stubs.

- [ ] Add assertions that arginfo return types match stub return types

## User Stories

### US-HF-1: Return type accuracy
> As a downstream consumer (doctrine-firebird-driver), I need
> `fbird_get_client_version()` to return `float` as declared, so that
> PHPStan and runtime type checks don't fail on unexpected `string`.

### US-HF-2: Parameter type safety
> As a PHP developer, I need arginfo to match actual parameter types,
> so that passing `Firebird\Statement` objects to `fbird_execute()` or
> `null` as `$params` doesn't cause TypeError or segfault.

### US-HF-3: Reflection accuracy
> As a proxy/mock framework user, I need `ReflectionFunction::getReturnType()`
> to report `object` (not `resource`) for functions that return
> `Firebird\*` objects, so that code generation produces correct types.

### US-HF-4: THROW mode reliability
> As a doctrine-firebird-driver developer, I need `FBIRD_EXCEPTION_MODE_THROW`
> to actually throw on ALL failure paths (not just Firebird status errors),
> so that `fbird_errcode()`/`fbird_errmsg()` surface the real cause instead
> of empty values.

### US-HF-5: BatchHandle ergonomics
> As an OOP API consumer, I need `Firebird\BatchHandle` to expose methods
> (not be an opaque marker), so that I can call `execute()`, `cancel()`,
> `add()` etc. directly without parallel procedural state tracking.

## Affected Files

| File | Change | HF |
|------|--------|----|
| `firebird.c` | arginfo fixes (return types, param types, nullable arrays) | HF-1,2,3 |
| `stubs/firebird-stubs.php` | return type + param type sync | HF-1,2 |
| `phpstan/fbird.stub.php` | return type sync (if exists) | HF-1 |
| `fbird_query_exec.c` | NULL-deref guards, THROW mode, type error | HF-2,4,6 |
| `fbird_transaction.c` | THROW mode, trans_start parse fix | HF-2,4 |
| `fbird_query_prepare.c` | THROW mode | HF-4 |
| `fbird_batch.c` | THROW mode | HF-4 |
| `fbird_connection.c` | THROW mode | HF-4 |
| `fbird_service.c` | THROW mode, silent RETURN_FALSE | HF-4 |
| `fbird_events.c` | THROW mode | HF-4 |
| `fbird_blobs.c` | THROW mode | HF-4 |
| `fbird_error.c` | THROW mode | HF-4 |
| `fbird_inspection.c` | silent RETURN_FALSE | HF-4 |
| `fbird_classes.c` | BatchHandle method entries | HF-5 |
| `stubs/firebird-classes.php` | BatchHandle methods | HF-5 |
| `tests/` | New regression tests + updates | All |

## Risks

| Risk | Mitigation |
|------|------------|
| arginfo change breaks callers | `mixed` accepts everything `IS_STRING` did |
| THROW mode change breaks SILENT users | SILENT mode unchanged (still warns) |
| BatchHandle methods break existing procedural API | Methods are additive (class was empty before) |
| Nullable array change allows null where before rejected | Stubs already say `?array` - arginfo now matches |

## Test Strategy

1. **TDD**: All regression tests committed before fixes (expected to FAIL)
2. **Per-issue tests**: Each HF item has dedicated .phpt test files
3. **Reflection tests**: Verify arginfo via `ReflectionFunction`/`ReflectionClass`
4. **THROW mode tests**: Verify exceptions + errcode/errmsg for each path
5. **NULL-deref tests**: Pass null as params, verify no segfault
6. **Full 12-container matrix**: `scripts/test_matrix.sh`
7. **CI gates**: `check-stubs-sync.sh`, `check-version-stamps.sh`, `check-clean-sections.sh`

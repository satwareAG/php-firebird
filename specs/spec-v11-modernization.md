---
description: >-
  v11.0 modernization: typed arginfo for all 84 procedural functions,
  resource-to-object migration, direct C calls in OOP layer, dead code removal.
tags: [modernization, arginfo, resource-to-object, breaking-change, v11]
priority: 11
---

> **Status: OPEN** - Milestone v11.0

# Spec: v11.0 Modernization

## Goal

Modernize the extension API to align with PHP 8.x best practices: typed return
values in arginfo, opaque objects instead of resources, and internal code cleanup.
This is a **breaking change** release.

## Success Criteria

- [ ] M2: All 84 procedural arginfo declarations use typed return macros
- [ ] M3: `fbird_connect()` returns `Firebird\Connection` object (not resource)
- [ ] M12: OOP layer calls internal C functions directly (no `call_user_function`)
- [x] L3: All dead `FB_API_VER < 30` code paths removed
- [ ] L2: `gds32_ms` fallback removed from `config.w32`
- [ ] Migration guide for v10.x to v11.0
- [ ] All stubs updated for new return types

## Part 1: M2 - Arginfo Typed Returns

Migrate all 84 `ZEND_BEGIN_ARG_INFO_EX` declarations to
`ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX` with correct return types.

Group by function family:
- Connection functions (8): `resource|false` to `Firebird\Connection|false`
- Transaction functions (6): `resource|false` to `Firebird\Transaction|false`
- Query functions (12): `resource|false` to mixed
- Result functions (8): `array|false`, `int`, `string`
- Blob functions (10): various
- Service functions (6): `resource|false`
- Batch functions (14): various
- Metadata functions (8): `array|false`
- Remaining (12): various

### Affected Files

- `firebird.c` - all arginfo declarations

## Part 2: M3 - Resource-to-Object Migration

Replace `zend_resource` returns with opaque `Firebird\*` objects in Layer 1
procedural functions. PHP upstream (ext/pgsql, ext/mysqli) has been doing this
since PHP 8.1.

### Breaking Changes

- `fbird_connect()` returns `Firebird\Connection` instead of `resource`
- `fbird_trans()` returns `Firebird\Transaction` instead of `resource`
- `is_resource()` checks will break - document `instanceof` alternative

### Affected Files

- `fbird_connection.c`, `fbird_transaction.c`, `fbird_query_exec.c`
- `fbird_query_prepare.c`, `fbird_blobs.c`, `fbird_service.c`
- `fbird_classes.c` - merge Layer 1 and Layer 2

## Part 3: M12 - Direct C Calls in OOP Layer

Replace `call_user_function()` in `fbird_classes.c` with direct calls to internal
C functions (`_php_fbird_connect`, `_php_fbird_prepare`, etc.).

### Affected Files

- `fbird_classes.c`

## Part 4: L3/L2 - Dead Code Removal

- Remove all `#if FB_API_VER < 30` branches (Firebird 3.0 is minimum)
- Remove `gds32_ms.lib` fallback from `config.w32` (Firebird 1.x era)

### Affected Files

- All `.c` and `.h` files with `FB_API_VER` guards
- `config.w32`

## Risks

| Risk | Mitigation |
|------|------------|
| Breaking `is_resource()` checks | Major version bump, migration guide |
| Arginfo changes affect Reflection | Test with `ReflectionFunction` |
| OOP layer direct calls may miss edge cases | Extensive test coverage already exists |

## Test Strategy

1. All 274 existing tests updated for object returns
2. New `resource_bc_break.phpt` verifying `instanceof` works
3. `ReflectionFunction` test for typed return info
4. Migration guide with before/after examples

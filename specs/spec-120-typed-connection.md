---
description: >-
  v10.0.0: fbird_connect() returns Firebird\Connection instead of
  opaque resource. Compatibility layer for existing code.
tags: [v10, breaking-change, issue-120, typed-objects]
priority: 2
---

> **Status: RESOLVED in v10.0.0** (2026-03-27)
> Firebird\Connection OOP class provides typed connection objects.
> fbird_connect() and fbird_pconnect() return Firebird\Connection objects.
> All three API layers (procedural, OOP, PDO) are fully functional.


# Spec #120: Typed Connection Objects

## Goal

Replace PHP resource-based "Firebird link" with `Firebird\Connection` objects as the
primary return type from `fbird_connect()` and `fbird_pconnect()`, while maintaining
backward compatibility for existing procedural code.

## Success Criteria

- [ ] `fbird_connect()` returns `Firebird\Connection` object (not resource)
- [ ] `is_resource($conn)` returns false; `($conn instanceof Firebird\Connection)` returns true
- [ ] All 87 `fbird_*` functions accept both objects and legacy resources (deprecation warning on resource)
- [ ] OOP API (`$conn->query()`, `$conn->prepare()`) works on connections from `fbird_connect()`
- [ ] `Firebird\Transaction`, `Firebird\Statement`, `Firebird\ResultSet`, `Firebird\Blob` are typed objects
- [ ] All 247 existing tests pass without modification
- [ ] New test suite covers object type checking

## Current State

- `fbird_connect()` returns `resource` of type "Firebird link" (registered as `le_link`)
- OOP classes exist (`Firebird\Connection`, etc.) but are separate from procedural API
- Procedural and OOP paths coexist but don't interoperate
- Resource stored in `fbird_db_link` struct with `isc_db_handle db_handle` + `fbc_connection` OO pointer

## Target State

```php
// Before (v9):
$link = fbird_connect($host, $db, $user, $pw);
// $link is resource

// After (v10):
$conn = fbird_connect($host, $db, $user, $pw);
// $conn is Firebird\Connection object
$conn instanceof Firebird\Connection; // true
is_resource($conn); // false

// Backward compat - resource still accepted (deprecated):
$link = fbird_connect_legacy($host, $db, $user, $pw); // explicit resource mode
// OR: existing code using resource helpers still works with E_DEPRECATED
```

## Design

### Resource-to-Object Migration

| Resource Type | Target Class | Internal Struct |
|---------------|-------------|-----------------|
| "Firebird link" | `Firebird\Connection` | `fbird_db_link` |
| "Firebird transaction" | `Firebird\Transaction` | `fbird_transaction` |
| "Firebird query" | `Firebird\Statement` | `fbird_query` |
| "Firebird result" | `Firebird\ResultSet` | (derived from query) |
| "Firebird blob" | `Firebird\Blob` | (opaque handle) |

### Compatibility Layer

1. `zend_parse_parameters` accepts `z` (resource) or `O` (object) via custom parser
2. Resource path emits `E_DEPRECATED` on PHP 8.4+
3. Internal `_php_fbird_get_link()` extracts `fbird_db_link*` from either type

### Implementation Order

1. `Firebird\Connection` wraps `fbird_db_link*` as object property
2. Update `fbird_connect()` / `fbird_pconnect()` / `fbird_create_database()` return paths
3. Update all `fbird_*` ZPP calls to accept both types
4. Port remaining resource types (Transaction, Statement, ResultSet, Blob)
5. Deprecate `is_resource()` checks in userland docs

## Affected Files

- `firebird.c` - `PHP_FE` return types, `ZEND_BEGIN_ARG_INFO`
- `fbird_connection.c` - `fbird_connect()` return path
- `fbird_classes.c` - object creation, `_fbird_get_link_from_obj()`
- `fbird_classes.h` - class registration macros
- `fbird_transaction.c` - accept Connection object
- `fbird_query_prepare.c` - accept Connection object
- `fbird_query_exec.c` - accept Connection/Transaction objects
- `fbird_result.c` - return ResultSet objects
- `fbird_blobs.c` - return Blob objects
- `stubs/firebird-stubs.php` - update return types
- `phpstan/fbird.stub.php` - update return types
- `stubs/firebird-classes.php` - verify class stubs match

## Risks

| Risk | Mitigation |
|------|------------|
| Breaking existing `is_resource()` checks | Dual-accept layer; deprecation not removal |
| PDO driver depends on resource | Update PDO to use object extraction |
| Serialization of Connection objects | Implement `__serialize`/`__unserialize` or forbid |
| Persistent connections (pconnect) | Object wraps persistent `fbird_db_link*` |

## Test Strategy

1. New test: `fbird_connect_returns_object.phpt` - assert instanceof
2. New test: `fbird_connect_compat_resource.phpt` - resource path with E_DEPRECATED
3. New test: `fbird_connect_oop_interop.phpt` - procedural connect + OOP method calls
4. Existing 247 tests pass unchanged (dual-accept layer)
5. PHPStan: stubs reflect `Firebird\Connection` return type
---
description: >-
  v10.0.0: remove legacy_handle_ patterns, modernize events,
  simplify multi-db transactions, clean deprecated internals.
tags: [v10, cleanup, legacy-removal, deprecation]
priority: 4
---

> **Status: RELEASED in v10.0.0** (2026-03-27)
> - FBIRD_API_MODE_LEGACY dead code removed from src/php_fbird_compat.h
> - get_statement_interface dead global removed from php_fbird_includes.h / firebird.c
> - firebird_legacy_wrappers.c excluded from build (linker fix)
> - isc_array_* retained: no Firebird OO API replacement exists (Firebird upstream limitation)


# Spec: v10 Legacy Pattern Removal

## Goal

Remove deprecated internal patterns that accumulated during v7-v9 migrations,
simplifying the codebase for long-term maintainability.

## Success Criteria

- [ ] No `legacy_handle_*` functions remain in any `.c` file
- [ ] Event system uses direct `isc_*` calls (no OO wrapper layer)
- [ ] Multi-db transaction support simplified or removed (document decision)
- [ ] All `@deprecated` internal functions removed or inlined
- [ ] Build passes with zero deprecation warnings from our own code

## Current State

Per DEPRECATION-AUDIT.md, legacy patterns include:

| Pattern | Location | Count |
|---------|----------|-------|
| `legacy_handle_*` | fbird_classes.c, fbird_connection.c | 8 functions |
| OO event wrappers | fbird_events.c | 6 wrappers |
| Multi-db transaction arrays | fbird_transaction.c | 3 functions |
| Deprecated param_info path | fbird_metadata.c | 1 function |
| v8 compat shims | firebird.c | 4 macros |

## Target State

### legacy_handle_* Removal

These wrappers convert between resource handles and OO objects. After #120 (typed
objects), they become unnecessary because the object IS the handle.

Action: Delete all `legacy_handle_*` functions. Call sites use object directly.
Research (docs/research/firebird-client-api-v5.md) confirms all Firebird handles are `void*` pointers, simplifying direct object storage.

### Event Modernization

Current: `fbe_*` wrappers in legacy_wrappers.c call `isc_*` directly.
Target: `fbird_events.c` calls `isc_*` directly, removing the indirection.
Firebird Events API uses `isc_event_counts`, `isc_que_events`, and `isc_cancel_events` (array-based, no complex structs), making direct calls straightforward.

```c
// Before: fbe_event_block(...) -> malloc -> isc_event_block(...)
// After:  inline isc_event_block(...) call in fbird_set_event_handler()
```

### Multi-db Transaction Decision

Two options:
- **A)** Remove: Single-db transactions only (matches 99% of usage)
- **B)** Keep: Expose as `Firebird\Transaction::multi()` OOP method

Recommendation: **Option A** - remove internal multi-db support. Users needing
distributed transactions should use application-level coordination.

## Affected Files

- `fbird_classes.c` - remove `legacy_handle_*` (8 functions)
- `fbird_classes_internal.h` - remove declarations
- `fbird_events.c` - inline isc_* calls, remove fbe_* dependency
- `fbird_transaction.c` - remove multi-db array handling
- `firebird.c` - remove v8 compat macros
- `fbird_metadata.c` - remove deprecated param_info path

## Risks

| Risk | Mitigation |
|------|------------|
| Multi-db removal breaks users | Check GitHub issues for multi-db usage; none found |
| Event tests fail | Run event_poller_wrapper.phpt after each change |

## Test Strategy

1. `grep -r "legacy_handle" *.c` returns 0 matches
2. Event tests pass: `event_poller_wrapper.phpt`
3. Full suite: 247 tests pass
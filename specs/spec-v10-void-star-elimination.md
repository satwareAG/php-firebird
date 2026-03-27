---
description: >-
  v10.0.0: replace void* opaque pointers in firebird_utils.h C API
  with typed forward-declared structs for compile-time safety.
tags: [v10, type-safety, refactoring, firebird-utils]
priority: 3
---

> **Status: RESOLVED in v10.0.0** (2026-03-27)
> firebird_utils_typed.h created with 12 typed opaque struct wrappers.
> Call sites can migrate incrementally using the zero-overhead inline wrappers.


# Spec: v10 void* Elimination

## Goal

Replace all `void*` parameters in the C API defined in `firebird_utils.h` with typed
opaque struct pointers, providing compile-time type safety across all call sites.

## Success Criteria

- [ ] Forward-declared opaque structs: `fbc_connection_t`, `fbt_transaction_t`, `fbs_statement_t`, `fbb_blob_t`
- [ ] All function signatures in `firebird_utils.h` use typed pointers instead of `void*`
- [ ] All call sites in `fbird_*.c` and `pdo_fbird/*.c` compile without casts
- [ ] Zero runtime behavior change
- [ ] All tests pass

## Current State

Every function in `firebird_utils.h` uses `void*` for handles:

```c
int fbc_disconnect(void* connection, ISC_STATUS* status_vector);
int fbt_commit(void* transaction, ISC_STATUS* status_vector);
int fbs_prepare(void* master, void* connection, void* transaction, ...);
```

Passing a transaction where a connection is expected compiles silently.

## Target State

```c
typedef struct fbc_connection_t fbc_connection_t;
typedef struct fbt_transaction_t fbt_transaction_t;
typedef struct fbs_statement_t fbs_statement_t;
typedef struct fbb_blob_t fbb_blob_t;

int fbc_disconnect(fbc_connection_t* connection, ISC_STATUS* status_vector);
int fbt_commit(fbt_transaction_t* transaction, ISC_STATUS* status_vector);
int fbs_prepare(fbc_master_t* master, fbc_connection_t* conn, fbt_transaction_t* trans, ...);
```

**Constraint**: `firebird_utils.h` and `firebird_utils.cpp` MUST NOT be modified in v10.
The typed structs are defined in a NEW header (`firebird_utils_typed.h`) that wraps
the existing `void*` API with type-safe inline functions.

## Design: Wrapper Header Approach

Since `firebird_utils.h` is frozen, create `firebird_utils_typed.h`:

```c
#ifndef FIREBIRD_UTILS_TYPED_H
#define FIREBIRD_UTILS_TYPED_H

#include "firebird_utils.h"

typedef struct fbc_connection_t { void* p; } fbc_connection_t;
typedef struct fbt_transaction_t { void* p; } fbt_transaction_t;
typedef struct fbs_statement_t { void* p; } fbs_statement_t;
typedef struct fbb_blob_t { void* p; } fbb_blob_t;

// Type-safe wrappers (inline, zero overhead)
static inline int fbc_disconnect_typed(fbc_connection_t* conn, ISC_STATUS* sv) {
    return fbc_disconnect(conn->p, sv);
}
// ... etc for all functions
#endif
```

Call sites migrate from `fbc_disconnect(ptr)` to `fbc_disconnect_typed(&typed_ptr)`.

## Affected Files

### New
- `firebird_utils_typed.h` - typed struct definitions + inline wrappers

### Modified (migration)
- `fbird_connection.c` - use typed wrappers
- `fbird_transaction.c` - use typed wrappers
- `fbird_query_prepare.c` - use typed wrappers
- `fbird_query_exec.c` - use typed wrappers
- `fbird_result.c` - use typed wrappers
- `fbird_query_bind.c` - use typed wrappers
- `fbird_blobs.c` - use typed wrappers
- `fbird_batch.c` - use typed wrappers
- `fbird_service.c` - use typed wrappers
- `fbird_events.c` - use typed wrappers
- `pdo_fbird/pdo_fbird_driver.c` - use typed wrappers
- `pdo_fbird/pdo_fbird_stmt.c` - use typed wrappers

## Risks

| Risk | Mitigation |
|------|------------|
| Inline function bloat | Compilers inline away; header is per-TU |
| Wrapper maintenance burden | Generate with script if needed |
| fb_ptr_t confusion | Typed structs coexist with fb_ptr_t during migration |

## Test Strategy

1. Build with `-Werror=implicit-function-declaration` passes
2. Intentionally swap connection/transaction in one call site - verify compile error
3. All 247 tests pass unchanged
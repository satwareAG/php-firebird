# Spec: v10.1.0 - FB Version-Gated Statement Cleanup

**Issues**: #139 (statement), #140 (ISC_TEB)
**Milestone**: v10.1.0
**Status**: RELEASED in v10.1.0

## Goal

Restore the server-side RAM fix from v10.0.1 (issue #135) for Firebird 4.0+ clients,
while keeping Firebird 3.0 clients safe via compile-time version gating.

## Problem Statement

- v10.0.1 used `IStatement::free()` and `IResultSet::close()` to release server resources (fixes #135)
- v10.0.2 reverted both to `release()` because FB 3.0 client segfaults (#137)
- Result: ~1.5 GB server RAM growth with thousands of DML calls for all FB 4+/5+ users

## Root Cause Analysis

### IResultSet::close() segfault on FB 3.0

When `fetchNext()` returns `RESULT_NO_DATA` (EOF), Firebird 3.0 implicitly closes the
cursor server-side. Calling `close()` again is a double-close that triggers use-after-free.

Firebird 4.0+ handles double-close gracefully.

### IStatement::free() segfault on FB 3.0

`IStatement::free()` (DSQL_drop equivalent) leaves the server connection in an invalid
state on FB 3.0 when the statement was associated with an uncommitted transaction.
This causes `IAttachment::detach()` to segfault during PHP resource cleanup.

Firebird 4.0+ handles this gracefully.

## Solution: Compile-Time Version Gating

Use `#if FB_API_VER >= 40` in `src/cpp/fb_statement.hpp`.

The CI matrix compiles once per (PHP version x FB client version):
- FB 3.0 client jobs get `release()` (safe)
- FB 4.0/5.0 client jobs get `free()`/`close()` (proper cleanup)

Precompiled bundles compiled against FB 5.0 client (FB_API_VER=50) will use `free()`/`close()`.
When a FB 5.0 client bundle connects to a FB 3.0 server, the OO API handles protocol
negotiation - the FB 5.0 client library handles `free()`/`close()` correctly regardless
of server version.

## Implementation: Issue #139 (fb_statement.hpp)

### closeCursor() change

```cpp
bool closeCursor(ISC_STATUS* status_vector) noexcept {
    if (!result_set_) {
        cursor_open_ = false;
        return true;
    }

    try {
#if FB_API_VER >= 40
        // FB 4.0+: Use close() for proper server-side cursor release.
        // close() = isc_dsql_free_statement(DSQL_close): explicitly closes the
        // server cursor and releases the interface. Fixes #135 RAM accumulation.
        //
        // cursor_open_ flag guards against double-close:
        // - If cursor_open_ is true: cursor is still active, close() is safe
        // - If cursor_open_ is false: fetchNext() reached EOF and FB implicitly
        //   closed the cursor server-side; release() avoids double-close crash
        if (cursor_open_) {
            Firebird::CheckStatusWrapper fb_status(&status_wrapper_);
            result_set_->close(&fb_status);
            if (statusHasError(&fb_status)) {
                if (status_vector) copyStatusToVector(&fb_status, status_vector);
                result_set_->release();
                result_set_ = nullptr;
                cursor_open_ = false;
                return false;
            }
            result_set_->release();
        } else {
            // Already at EOF: cursor was implicitly closed by FB server.
            // release() decrements refcount only (no double-close).
            result_set_->release();
        }
#else
        // FB 3.0: release() only (close() segfaults on double-close after EOF).
        result_set_->release();
#endif
        result_set_ = nullptr;
        cursor_open_ = false;
        return true;

    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_except2;
            status_vector[2] = isc_arg_end;
        }
        result_set_ = nullptr;
        cursor_open_ = false;
        return false;
    }
}
```

### free() change

```cpp
bool free(ISC_STATUS* status_vector) noexcept {
    closeCursor(status_vector);

    if (!statement_) {
        prepared_ = false;
        return true;
    }

    try {
#if FB_API_VER >= 40
        // FB 4.0+: Use free() for proper server-side statement release.
        // free() = isc_dsql_free_statement(DSQL_drop): explicitly destroys the
        // prepared statement on the server. Fixes #135 RAM accumulation for DML.
        Firebird::CheckStatusWrapper fb_status(&status_wrapper_);
        statement_->free(&fb_status);
        if (statusHasError(&fb_status)) {
            if (status_vector) copyStatusToVector(&fb_status, status_vector);
            statement_->release();
            statement_ = nullptr;
            prepared_ = false;
            return false;
        }
        statement_->release();
#else
        // FB 3.0: release() only (free() segfaults with uncommitted transactions).
        statement_->release();
#endif
        statement_ = nullptr;
        prepared_ = false;
        return true;

    } catch (...) {
        if (status_vector) {
            status_vector[0] = isc_arg_gds;
            status_vector[1] = isc_except2;
            status_vector[2] = isc_arg_end;
        }
        statement_ = nullptr;
        prepared_ = false;
        return false;
    }
}
```

**Note**: `status_wrapper_` must be added as a member, initialized from IMaster. OR use
`IMaster::getStatus()` at call site. Check existing pattern in `prepare()` method.

**Actual implementation note**: The `closeCursor()` and `free()` methods do NOT have access
to `IMaster*`. Looking at the existing code, they do not take `IMaster*` as parameter. The
`status_wrapper_` field does not exist in StatementWrapper. The `CheckStatusWrapper` requires
an `IStatus*`. We need to get it from somewhere. Looking at how other methods work:

The existing `closeCursor()` and `free()` do NOT use `IStatus` - they just call `release()`
which is a reference-count method that doesn't need status. For `close()` and `free()` that
DO need status, we need to store a master or status pointer.

**Revised approach**: Add a `Firebird::IMaster*` member to StatementWrapper (set during
`prepare()`), then use it in `closeCursor()` and `free()`.

## Implementation: Issue #140 (fbird_transaction.c)

Remove from `PHP_FUNCTION(fbird_trans)`:

1. Remove `ISC_TEB *teb;` declaration in the `if (argn > 0)` block
2. Remove `teb = (ISC_TEB *) safe_emalloc(sizeof(ISC_TEB),argn,0);` 
3. Remove the TEB population loop body (the `teb[link_cnt].db_ptr = ...` line)
4. The `tpb_len` and `tpb_ptr` references in the single-connection path: update to use
   the tpb buffer directly (it's already `tpb[TPB_MAX_SIZE * 0]`)
5. Remove `efree(teb)` calls throughout

The single-connection OO API path (`link_cnt == 1`) accesses:
- `teb[0].tpb_len` - replace with local `tpb_len` variable
- `teb[0].tpb_ptr` - replace with `&tpb[0]`

## Success Criteria

- All 12 CI jobs pass (PHP 8.2-8.5 x FB 3.0/4.0/5.0)
- FB 3.0 jobs: no segfaults, same behavior as v10.0.2
- FB 4.0/5.0 jobs: server RAM does not grow unboundedly with repeated DML
- New test `fbird_query_stmt_release_002.phpt` passes on FB 4.0/5.0, skips on FB 3.0

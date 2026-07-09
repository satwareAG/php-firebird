# ADR: PDO Multiple Active Result Sets (MARS) — #435

## Status
Proposed — architectural analysis complete, implementation not started.

## Context

Issue #435 requires multiple `PDOStatement` objects to be active simultaneously
on a single `PDO` connection. Currently, the second `prepare()` + `execute()`
closes the first statement's cursor, making interleaved fetches impossible.

## Root Cause

`pdo_fbird/pdo_fbird_stmt.c:222-225` (added in commit `30e852a`, the initial
PDO driver implementation, March 2026):

```c
/* Close any previously open cursor */
if (fbs_is_cursor_open(S->fbs_stmt)) {
    fbs_close_cursor(S->fbs_stmt, S->status);
}
```

This closes the cursor on the **current statement** before re-executing it.
However, the problem is broader: each statement shares the connection's
single transaction handle (`H->fbt_trans`), and Firebird cursors are
associated with transactions. When a second statement executes on the same
transaction, the first cursor remains valid in Firebird's API — the
close-on-execute is an **artificial restriction** in the PHP driver, not a
Firebird limitation.

## Architecture

### Per-statement resources (NOT shared)
Each `pdo_fbird_stmt` has its own:
- `fbs_stmt` — `fb::Statement*` (prepared statement handle)
- `out_buf` / `in_buf` — message buffers (allocated per-statement)
- `out_meta` / `in_meta` — metadata
- `has_rows` — cursor state
- `scrollable` — cursor type

### Shared resources (the constraint)
- `H->fbt_trans` — single `fb::Transaction*` per connection
- `H->fbc_conn` — single `fb::Connection*` per connection

Firebird supports multiple open cursors per transaction natively. The
constraint is that all cursors share the same transaction — if the
transaction commits or rolls back, all cursors are invalidated.

## Implementation Options

### Option A: Remove close-on-execute (minimal)

Remove the `fbs_close_cursor()` call at line 222-225. Each statement manages
its own cursor independently. The cursor stays open until `closeCursor()` is
called or the statement is destroyed.

**Risk:** The close-on-execute was likely added as a safety measure. Without
it, re-executing the same statement (e.g., `$stmt->execute()` twice) would
need to close the previous cursor. The fix is to close only when re-executing
the **same** statement, not when executing a **different** statement.

```c
/* Close cursor only if re-executing the same statement */
if (S->cursor_executed && fbs_is_cursor_open(S->fbs_stmt)) {
    fbs_close_cursor(S->fbs_stmt, S->status);
}
S->cursor_executed = 1;
```

**Pros:** Minimal change, backward compatible, enables MARS.
**Cons:** Need to verify no buffer conflicts when two statements are open.

### Option B: Cursor pool per connection (full)

Track all open statements on the connection. Close them only when the
connection is destroyed or when explicitly closed by the user.

**Pros:** Explicit lifecycle management.
**Cons:** More complex, potential memory leaks if statements aren't closed.

### Option C: PDO attribute gate (backward compat)

Add `PDO::FBIRD_ATTR_MULTI_STATEMENTS` (default off). When enabled, skip the
close-on-execute. When disabled, keep current behavior.

**Pros:** No behavior change for existing code. Opt-in for MARS.
**Cons:** Adds configuration complexity.

## Recommendation

**Option A** with a re-execution guard. The close-on-execute should only
apply when the **same statement** is re-executed, not when a **different
statement** is executed on the same connection. This is the standard behavior
in pdo_mysql and pdo_pgsql.

## Test Plan

1. Prepare two SELECT statements on one connection
2. Execute both
3. Fetch interleaved: stmt1->fetch(), stmt2->fetch(), stmt1->fetch()
4. Verify both return correct rows
5. Verify closeCursor() on one doesn't affect the other
6. Verify transaction commit invalidates both cursors
7. Verify re-executing the same statement closes its previous cursor

## Implementation Steps

1. Add `cursor_executed` flag to `pdo_fbird_stmt` struct
2. Modify `pdo_fbird_stmt_execute()`: close cursor only if `cursor_executed`
3. Set `cursor_executed = 1` after successful execute
4. Reset `cursor_executed = 0` in `closeCursor()`
5. Write MARS test file
6. Run full test suite to verify no regression

## References

- Issue: #435
- Original spec: #322 (PDO conformance)
- Close-on-execute: `pdo_fbird/pdo_fbird_stmt.c:222-225` (commit `30e852a`)
- Shared transaction: `pdo_fbird/php_pdo_fbird_int.h:16` (`fbt_trans`)
- Stubs note: `stubs/pdo-fbird-stubs.php:197-198` ("Not yet implemented")

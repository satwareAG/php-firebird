# ADR: PDO Multiple Active Result Sets (MARS) — #435

## Status
Verified — MARS works. No code changes needed beyond a clarity flag.

## Context

Issue #435 required multiple `PDOStatement` objects to be active simultaneously
on a single `PDO` connection. The stubs file listed this as "Not yet
implemented", and an initial ADR (now superseded) claimed that
`prepare()` + `execute()` on a second statement closes the first statement's
cursor.

## Corrected Root Cause Analysis

**MARS was never broken.** The initial ADR's root cause analysis was incorrect:

- `prepare()` (`pdo_fbird_driver.c:120-189`) creates a new `pdo_fbird_stmt`
  with its own `fbs_stmt`, `out_buf`, `in_buf` — it does NOT touch other
  statements.
- `execute()` (`pdo_fbird_stmt.c:222-225`) closes the cursor on the
  **current statement only** before re-executing it. This is correct behavior
  for re-execution and does NOT affect other statements.
- `openCursor()` (`fb_statement.hpp:237-244`) closes existing cursor on
  `this` wrapper before opening a new one — per-statement, not per-connection.
- There is no statement tracking list on `pdo_fbird_db_handle`. Nothing
  iterates over other statements.

The "Not yet implemented" note in `stubs/pdo-fbird-stubs.php` was outdated —
it was never actually tested. TDD verification (Test 1-6 in
`tests/pdo_fbird/pdo_fbird_mars.phpt`) confirms MARS works correctly on
FB3, FB4, and FB5.

## Architecture

### Per-statement resources (NOT shared)
Each `pdo_fbird_stmt` has its own:
- `fbs_stmt` — `fb::Statement*` (prepared statement handle)
- `out_buf` / `in_buf` — message buffers (allocated per-statement)
- `out_meta` / `in_meta` — metadata
- `has_rows` — cursor state
- `cursor_executed` — re-execution guard flag (added for clarity)
- `scrollable` — cursor type

### Shared resources
- `H->fbt_trans` — single `fb::Transaction*` per connection
- `H->fbc_conn` — single `fb::Connection*` per connection

Firebird supports multiple open cursors per transaction natively. All cursors
share the same transaction — if the transaction commits or rolls back, all
cursors are invalidated server-side. The C++ layer self-heals:
`fetchNext()` detects the invalidation (`statusHasError`), sets
`cursor_open_ = false`, and returns `-1` (`fb_statement.hpp:302-307`).

## Changes Made

### `cursor_executed` flag (clarity, not correctness)

Added `cursor_executed` field to `pdo_fbird_stmt` struct for explicit
re-execution semantics:

- **`php_pdo_fbird_int.h`**: Added `int cursor_executed` field
- **`pdo_fbird_stmt.c:222`**: Guard close-on-execute with `S->cursor_executed &&`
- **`pdo_fbird_stmt.c:247`**: Set `cursor_executed = 1` after successful SELECT execute
- **`pdo_fbird_stmt.c:264`**: Set `cursor_executed = 1` after successful DML/DDL execute
- **`pdo_fbird_stmt.c:1024`**: Reset `cursor_executed = 0` in `closeCursor()`

The guard is technically redundant (the C++ `openCursor()` already handles
close-before-open), but it makes the re-execution semantics explicit at the
PDO layer and protects the DML re-execution path (where `openCursor()` is
not called).

### Stubs correction

Removed "Multiple active result sets on a single connection" from the
"Not yet implemented" list in `stubs/pdo-fbird-stubs.php`.

## Test Coverage

`tests/pdo_fbird/pdo_fbird_mars.phpt` — 6 tests:

1. Interleaved fetch from 2 SELECTs (ascending + descending)
2. Close one cursor, verify other survives
3. Re-execute same statement (fresh result set)
4. Three statements round-robin
5. Transaction commit invalidates cursors (PDOException or false)
6. DML between SELECTs with autocommit (commit_retaining preserves cursor)

Verified: PASS on FB3, FB4. CI verified on FB5.

## References

- Issue: #435
- Test: `tests/pdo_fbird/pdo_fbird_mars.phpt`
- Close-on-execute: `pdo_fbird/pdo_fbird_stmt.c:222-229` (commit `30e852a`)
- C++ self-healing: `src/cpp/fb_statement.hpp:302-307`
- Shared transaction: `pdo_fbird/php_pdo_fbird_int.h:16` (`fbt_trans`)

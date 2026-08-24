# Upgrading Guide

This guide covers behavior changes between php-firebird versions that may
affect existing applications.

## v13.2.x → Unreleased (PR #590)

### `fbird_query(FBIRD_CREATE, ...)` returns a `Firebird\Connection` object

Aligning the runtime with the declared arginfo (#306 follow-up; debug/ASAN
builds fatal on the old raw-resource return), the deprecated
`fbird_query(FBIRD_CREATE, $sql)` form now returns the same object type as
`fbird_connect()`. Truthiness checks are unaffected; `is_resource()` checks
must switch to `instanceof Firebird\Connection`. The object works everywhere
the resource did (dual-accept: `fbird_query`, `fbird_close`, `fbird_drop_db`,
transactions). Prefer `fbird_create_database()`, which is not deprecated.

## v13.2.x → Unreleased (#589)

### `fbird_rollback_ret()` with no open cursors is now a hard rollback

Same shape as the #586 commit_ret change: a retaining rollback with **zero
open cursors** performs a hard rollback + transparent restart (stored TPB),
releasing the attachment-level relation locks that isc_rollback_retaining
otherwise keeps until disconnect. Callers keeping cursors open retain true
retaining semantics.

**BLOB handles** (same hazard as #586): a hard rollback invalidates open
BLOB handles on that transaction; `isc_rollback_retaining` preserved them.
The legacy blob read-loop with periodic `fbird_rollback_ret()` must keep a
SELECT cursor open on the same transaction or restructure around plain
`fbird_rollback()` boundaries.

## v13.2.x → Unreleased (#595)

### `fbird_connect()`/`fbird_pconnect()` gain a documented `sync` parameter

The C layer always accepted nine positional parameters
(`database, username, password, charset, buffers, dialect, role, sync, flags`),
but stubs/arginfo documented only eight. Consequences of the old signature:

- Passing `FBIRD_CONNECT_FORCE_NEW` as the **8th** argument silently set the
  lock timeout (`sync`) to `2` instead of forcing a new connection.
- Debug/ASAN PHP builds fatalled on calls reaching deep parsing.

Insert your intended flags value as the **9th** argument (with `0` for
`sync` unless you deliberately set the lock timeout). Keyword-style callers
using at most 7 arguments are unaffected.

## v13.2.x → Unreleased (PR #588)

### `fbird_commit_ret()` with no open cursors is now a hard commit

To stop attachment-level relation lock retention (#578/#586), a retaining
commit on a transaction with **zero open cursors** now performs a hard
commit + transparent restart (handle stays valid, stored TPB is restored).

**BLOB handles**: a hard commit invalidates open BLOB handles on that
transaction; `isc_commit_retaining` preserved them. If you use the legacy
chunked-blob-import pattern, either keep a SELECT cursor open on the same
transaction (takes the retaining path, released lazily when the cursor
closes) or restructure around plain `fbird_commit()` boundaries:

```php
// Affected: no cursor open -> hard commit -> $blob handle dies
$blob = fbird_blob_create($c, $tx);
// ... add segments ...
fbird_commit_ret($tx);          // blob handle now invalid

// Unaffected patterns:
// 1. any open SELECT cursor on $tx at commit_ret time (retaining path)
// 2. PDO firebird (pdo_fbird) - calls retaining commit directly
```

## v13.0.x → v13.2.x

### Default transaction isolation: SNAPSHOT

The default transaction uses **SNAPSHOT (CONCURRENCY)** isolation when
`fbird.default_trans_params=0` (the default). This means each transaction
sees a frozen view of the database as of the transaction start time.

Under snapshot isolation, DDL (CREATE TABLE, ALTER TABLE, etc.) committed
by **other** transactions after your transaction started is **not visible**
until your transaction ends and a new one begins.

For DDL-heavy workflows (e.g., Doctrine migrations), consider switching to
READ COMMITTED isolation:

```php
// Option A: Global INI setting
// fbird.default_trans_params = 8   (FBIRD_COMMITTED)

// Option B: Per-transaction
$tx = fbird_trans($conn, [FBIRD_COMMITTED]);
```

### `fbird.auto_ddl_commit` INI directive (new in v13.2.0)

A new INI setting `fbird.auto_ddl_commit` controls whether DDL on
**explicit transactions** is automatically committed to release metadata
locks from open cursors.

| Value | Behavior |
|-------|----------|
| `0` (default) | DDL on explicit transactions stays atomic (transactional DDL). Only fires transparent commit+restart when `open_cursor_count > 0` (open SELECT cursors holding metadata locks). |
| `1` | DDL on explicit transactions **always** fires transparent commit+restart, regardless of open cursors. This matches the v13.1.0 behavior. |

**Important**: This INI only affects **explicit transactions** (transactions
started via `fbird_trans()` / `fbird_query($tx, $sql)`). The
**autocommit/default transaction** path (`fbird_query($conn, $sql)`) is
unaffected — it always commits after each non-SELECT statement (Issue #294).

### #294 autocommit: error reporting (changed in v13.2.7)

When `fbird_query($conn, $sql)` is called without an explicit transaction,
the default transaction is automatically committed after each non-SELECT
statement. Previously, if this autocommit **failed**, the error was
silently swallowed and the statement was rolled back without any indication.

As of v13.2.7, autocommit failures are now reported:

- In **exception mode** (`fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW)`):
  a `FirebirdException` is thrown.
- In **warning mode** (default): an `E_WARNING` is emitted, and
  `fbird_errmsg()` returns the error message.

For DDL statements specifically, if the autocommit failure is caused by
open cursors from prior SELECTs on the same default transaction, the
extension now closes those cursors and retries the commit before reporting
an error.

### `fbird_release_metadata_locks()` (new in v13.2.0)

New procedural and OOP API for explicitly releasing metadata locks held
by open cursors on a transaction:

```php
// Procedural
fbird_release_metadata_locks($tx);

// OOP
$tx->releaseMetadataLocks();
```

This performs a hard commit (releasing all locks) and restarts the
transaction with the original TPB (isolation level, access mode, etc.).
Use this before DDL if you have open cursors from schema introspection
and want manual control over lock release.

### Cursor-count gating (#566)

In v13.1.0, the transparent DDL commit+restart on explicit transactions
fired on **all** DDL statements. This broke transactional DDL semantics:
DML before DDL in the same transaction got committed unexpectedly.

As of v13.2.0, the transparent commit+restart only fires when
`open_cursor_count > 0` — i.e., when there are unfreed SELECT cursors
that could be holding metadata locks. DDL after DML (without open
cursors) stays atomic within the transaction.

If you relied on the v13.1.0 unconditional behavior, set
`fbird.auto_ddl_commit=1`.

## v12.x → v13.0.x

The extension was rewritten from the legacy `ext/interbase` codebase to
use the modern Firebird OO API (C++17). The `fbird_*` procedural API,
`Firebird\*` OOP classes, and `pdo_fbird` PDO driver are all new code.

Key changes:
- PHP 8.2+ required (was 7.4+)
- Firebird 3.0+ required (was 2.5+)
- `fbird_connect()` returns `Firebird\Connection` objects (not resources)
- Exception mode available via `fbird_set_exception_mode()`
- PDO driver is a separate extension (`pdo_fbird.so`)

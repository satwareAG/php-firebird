---
description: >-
  v12.1.0 Procedural API parity: close gaps between php-firebird's fbird_*
  procedural API and the big-5 PHP database extensions (mysqli, pgsql, sqlite3,
  oci8). Each gap gets a RED test (with --SKIPIF--) in this milestone;
  implementation spawns separate feat/* branches (M9 backlog) after v12.1.0
  ships the test suite.
tags: [procedural, parity, testing, fb3, v12.1]
priority: 2
---

# Spec: v12.1.0 Procedural API Parity

## Issue
#323 (this spec) — index for #359-#384 (26 test/feat issues)

## Branch
`test/integration-conformance`

## Date
2026-07-07

## Status
Approved — execution in progress

---

## Intent

Close the capability gaps between php-firebird's `fbird_*` procedural API and
the procedural/database extensions of the "big 5" SQL databases that PHP
developers use: **mysqli** (MySQL/MariaDB), **pgsql** (PostgreSQL), **sqlite3**
(SQLite), **oci8** (Oracle), and the legacy **interbase** (the predecessor that
php-firebird replaced).

This milestone produces RED tests (using `--SKIPIF--` so CI stays green) that
document every gap. Implementation of each gap spawns a separate `feat/*` branch
in the M9 backlog after v12.1.0 ships. The goal is a public, searchable, +1-able
record of what needs to be built to make php-firebird feel familiar to PHP devs
migrating from MySQL, PostgreSQL, or Oracle.

---

## Background

### php-firebird unique strengths (preserve these)

The procedural API already exceeds the big-5 in several areas:
- **Driver-level backup/restore** (`fbird_backup`/`fbird_restore`) — no big-5 has this
- **Most granular transaction control** (full TPB, table reservations, multi-DB 2PC)
- **Native event manager** (`fbird_set_event_handler`/`fbird_poll_event`) — richer than pgsql LISTEN/NOTIFY
- **Limbo/2PC transaction recovery** (`fbird_get_limbo_transactions`/`fbird_reconnect_transaction`)
- **Batch DML API** (`fbird_batch_*`, FB 4.0+ IBatch)
- **Native SQL_ARRAY field support** (full read/write)
- **Service manager** (server_info, db_info, maintain_db, user management)
- **Scrollable cursors** (in PDO layer, procedural doesn't expose yet)

### Gap categories

The gaps fall into 5 categories:

1. **Result-set navigation** (most-felt by PHP devs): `fbird_fetch_array` (BOTH mode, regression vs interbase), `fbird_fetch_all`, `fbird_fetch_column`, `fbird_data_seek`, `fbird_fetch_object($class, $args)`
2. **Connection/session management**: `fbird_ping` (procedural), `fbird_set_charset`, `fbird_server_version` at connection level
3. **Prepared statement ergonomics**: `fbird_bind_param` (named/typed by ref), `fbird_bind_result`, `fbird_stmt_attr_get/set`, `fbird_result_metadata`, `fbird_send_long_data`, `fbird_stmt_reset`
4. **Error handling**: per-connection/per-statement error context (architectural), `fbird_error_list`, structured diagnostic fields
5. **Misc**: `fbird_multi_query`, `fbird_meta_data`/`fbird_list_tables`, `fbird_escape_literal`/`fbird_escape_identifier`, `fbird_blob_truncate`/`fbird_blob_export`, `fbird_debug`/`fbird_trace`

---

## Capability Matrix

Legend: **Y** = supported | **N** = not supported | **P** = partial / different API | **-** = N/A for that DB

| Capability | mysqli | pgsql | sqlite3 | oci8 | interbase | **php-firebird** |
|---|---|---|---|---|---|---|
| fetch_array (BOTH) | Y | Y | Y | Y | Y | **N** (#359 regression) |
| fetch_all | Y | Y | Y | Y | N | **N** (#363) |
| fetch_column | Y | P | N | P | N | **N** (#364) |
| fetch_object(class, args) | Y | Y | Y (CLASS) | N | N | **N** (#365) |
| data_seek (row) | Y | Y | P | N | N | **N** (#362) |
| ping (procedural) | Y | Y | - | - | N | **N** (#360) |
| set_charset / get_charset | Y | Y | - | N | N | **N** (#366) |
| server_version (conn) | Y | Y | Y | Y | N | **N** (#361) |
| bind_param (named, by ref) | Y | N (array) | Y | Y | N (varargs) | **N** (#367) |
| bind_result / define | Y | N | N | Y | N | **N** (#368) |
| per-conn error context | Y | Y (result) | N | Y (stmt) | N | **N** (#369 global) |
| error_list (array) | Y | P | N | P | N | **N** (#370) |
| structured diag fields | N | Y | N | P | N | **N** (#371) |
| multi_query | Y | P | Y (exec) | N | N | **N** (#372) |
| meta_data / list_tables | N (SQL) | Y | N | N (SQL) | N | **N** (#373) |
| escape_literal / identifier | N | Y | N | N | N | **N** (#374) |
| blob_truncate / erase | N | Y | N | Y | N | **N** (#375) |
| blob_export (to file) | N | Y | N | Y | N | **N** (#376) |
| debug / trace | Y | Y | N | N | N | **N** (#377) |
| stmt_attr_get/set | Y | N | N | Y | N | **N** (#378) |
| result_metadata (pre-exec) | Y | N | N | Y | N | **N** (#379) |
| send_long_data (stream) | Y | N | N | Y | N | **N** (#380) |
| stmt_reset | Y | N | Y | N | N | **N** (#381) |
| warning_count | Y | N | N | N | N | **-** (#382 engine N/A) |
| select_db | Y | - | - | - | N | **-** (#383 FB N/A) |
| thread_id | Y | N | N | N | N | **-** (#384 FB N/A) |

---

## User Stories / Acceptance Goals

### Goal 1: Close regression vs interbase (P0)

**Given** a PHP dev migrating from legacy `ext/interbase` to `php-firebird`
**When** they call `fbird_fetch_array($result)` expecting BOTH-mode (numeric + assoc keys)
**Then** it returns an array with both key types (currently fails — function doesn't exist)

**Success Criteria**:
- [ ] #359 RED test written for `fbird_fetch_array`
- [ ] M9 implementation branch `feat/v13.0-fetch-array-both-mode` tracked

### Goal 2: Per-connection error context (P0)

**Given** two concurrent Firebird connections on different threads (long-running daemon)
**When** query on conn1 fails, then query on conn2 fails
**Then** `fbird_errmsg($conn1)` returns conn1's error, `fbird_errmsg($conn2)` returns conn2's error (currently both return conn2's error — global single-slot)

**Success Criteria**:
- [ ] #369 RED test written demonstrating the race
- [ ] M9 implementation branch `feat/v13.0-per-conn-error-context` tracked

### Goal 3: Doctrine DBAL compatibility helpers (P1)

**Given** Doctrine DBAL's `SchemaManager` needs table/column metadata
**When** it calls the driver for `listTables()` / `listTableColumns()`
**Then** the driver provides structured metadata without requiring raw SQL against `RDB$*` tables

**Success Criteria**:
- [ ] #373 RED test for `fbird_meta_data` / `fbird_list_tables` / `fbird_list_fields`
- [ ] #361 RED test for `fbird_server_version` (Doctrine `getDatabasePlatformVersion()`)

### Goal 4: All other gaps documented as RED tests (P2-P3)

**Given** a PHP dev comparing php-firebird docs to mysqli/pg documentation
**When** they look for a function that exists in other drivers
**Then** they find either the function (green) or a tracked gap with priority (RED test)

**Success Criteria**:
- [ ] All 26 issues (#359-#384) have RED tests written and passing (via --SKIPIF--)

---

## Out of Scope

- Implementation of any gap (M9 `feat/*` branches)
- OOP `Firebird\*` class API gaps (covered by M4 client coverage)
- PDO driver gaps (covered by M1 PDO conformance)
- FB 4.0+ features (M8 stretch)

---

## Constitution Alignment

| Article | Requirement | How Met |
|---------|-------------|---------|
| II: Test-First | Tests before implementation | All 26 issues are RED tests first; gap implementations are M9 |
| VI: Baby Steps | <200 LOC per commit | Each test file is small (< 100 LOC) |
| VII: Coverage Gate | Coverage >= 60% | RED tests contribute to coverage even with --SKIPIF-- |

---

## Dependencies

- **Requires**: #317 (procedural-parity CI job with continue-on-error), #321 (--CLEAN-- enforcement)
- **Blocks**: M9 (implementation branches need the RED tests to turn green)

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| #359 fetch_array: adding new function is technically breaking (new global symbol) | Low | Low | Document as v13.0 breaking change; no existing code can break |
| #369 per-conn error: changing fbird_errmsg signature is breaking | High | High | Make optional arg BC: `fbird_errmsg($obj = null)` — null falls back to global |
| #367 bind_param: Firebird DSQL uses positional ? only, no native named params | Medium | Medium | Driver converts named to positional internally (like bundled pdo_firebird does) |

---

## Open Questions

- [ ] Should `fbird_fetch_array` use FBIRD_BOTH as default (matching interbase) or require explicit $fetch_type? **Recommended**: default FBIRD_BOTH for BC with interbase migration code.
- [ ] Should `fbird_bind_param` support named params (converting to ? internally) or positional only? **Recommended**: support both — Firebird SQL uses ? but Doctrine DBAL uses :name.

---

## Issue Index

### Feat issues (gap closures — RED test + M9 implementation tracking)

| Issue | Title | Priority | Target |
|-------|-------|----------|--------|
| #359 | feat: fbird_fetch_array (BOTH mode) — REGRESSION | P0 | v13.0 |
| #360 | feat: fbird_ping (procedural) | P1 | v12.1.x |
| #361 | feat: fbird_server_version at conn level | P1 | v12.1.x |
| #362 | feat: fbird_data_seek (scrollable) | P2 | v13.0 |
| #363 | feat: fbird_fetch_all | P2 | v13.0 |
| #364 | feat: fbird_fetch_column | P2 | v13.0 |
| #365 | feat: fbird_fetch_object(class, args) | P2 | v13.0 |
| #366 | feat: fbird_set_charset / get_charset | P2 | v13.0 |
| #367 | feat: fbird_bind_param (named, by ref) | P2 | v13.0 |
| #368 | feat: fbird_bind_result / define | P2 | v13.0 |
| #369 | feat: per-conn error context | P0 | v13.0 |
| #370 | feat: fbird_error_list | P2 | v13.0 |
| #371 | feat: structured diag fields | P2 | v13.0 |
| #372 | feat: fbird_multi_query + more_results | P2 | v13.0 |
| #373 | feat: fbird_meta_data / list_tables | P1 | v13.0 |
| #374 | feat: fbird_escape_literal / identifier | P3 | v13.0 |
| #375 | feat: fbird_blob_truncate / erase / flush | P3 | v13.0 |
| #376 | feat: fbird_blob_export (to file) | P3 | v13.0 |
| #377 | feat: fbird_debug / trace | P3 | v13.0 |
| #378 | feat: fbird_stmt_attr_get/set | P3 | v13.0 |
| #379 | feat: fbird_result_metadata | P3 | v13.0 |
| #380 | feat: fbird_send_long_data | P3 | v13.0 |
| #381 | feat: fbird_stmt_reset | P3 | v13.0 |

### Legitimate non-gap documentation (skip tests)

| Issue | Title | Why N/A |
|-------|-------|---------|
| #382 | skip: fbird_warning_count | Firebird has no warning stream |
| #383 | skip: fbird_select_db | DB bound to attachment |
| #384 | skip: fbird_thread_id | FB protocol N/A |

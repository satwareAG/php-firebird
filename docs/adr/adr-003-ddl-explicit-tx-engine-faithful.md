# ADR-003: DDL on explicit transactions mirrors engine semantics (no auto-commit)

Date: 2026-10-20
Status: Accepted
Supersedes: the expectation in #628 that same-tx DML after uncommitted DDL should work

## Context

#628 reported that DDL executed on an explicit transaction appears "silently lost":
a same-transaction `INSERT` prepare fails with `-204 Table unknown`, and the report
claimed the table "does not exist after commit" and that "the engine supports"
CREATE-then-INSERT inside one transaction (citing isql).

Investigation (2026-10-20, php84-fb3-dev + php85-fb5-dev, raw C client cross-check):

| Probe | Result |
|-------|--------|
| isql `SET AUTODDL OFF; SET TRANSACTION; CREATE TABLE; INSERT` | `-204 Table unknown` (byte-identical to the extension) |
| isql default (`AUTODDL ON`) | "works" - because isql **auto-commits every DDL statement**; the INSERT then compiles against a committed table in a fresh transaction |
| Raw C client (legacy `isc_dsql_*`, INET, SNAPSHOT and RC\|REC_VERSION\|WAIT TPBs, immediate and prepared) | `-204` in every combination |
| Extension (OO API, explicit tx) | `-204` in every combination, FB3 + FB5 |
| `fbird_rollback` after same-tx CREATE | table gone (DDL was transactional in the tx) |
| `fbird_commit` after same-tx CREATE | table persists (`SELECT COUNT(*)` works, post-commit `DROP TABLE` works) |

The engine does not compile DML against a relation created in the same
uncommitted transaction. The earlier "engine supports it" evidence was an isql
`AUTODDL ON` artifact. The "-607 Table does not exist after commit" observation
did not reproduce: the committed CREATE persists.

## Decision

The extension keeps **engine-faithful** semantics for DDL on explicit
transactions:

1. DDL executes transactionally in the caller's transaction (rollback drops it,
   commit persists it). No transparent auto-commit is added.
2. Same-tx DML against not-yet-committed DDL fails with the engine's precise
   `-204 Table unknown` error - a PHP warning, never silent.
3. The opt-in escape hatches remain:
   - `firebird.auto_ddl_commit=1` (transparent commit+restart before DDL, v13.1.0 legacy behavior)
   - commit the transaction (or run the DDL on its own transaction) before compiling dependent DML
   - `fbird_release_metadata_locks()` for explicit metadata-lock control (#566)

## Consequences

- Authors using transactional DDL must commit before compiling dependent DML -
  identical to working with isql under `SET AUTODDL OFF`.
- `tests/issue628_ddl_explicit_tx.phpt` locks this behavior in as a regression
  guard (fail-loudly -204 + persistence after commit + rollback drops).
- A future engine that lifts the same-tx compile limitation may require
  revisiting the -204 expectation in that test.

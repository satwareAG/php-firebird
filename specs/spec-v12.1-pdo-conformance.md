---
description: >-
  v12.1.0 PDO Definition conformance: verify every mandatory and optional PDO
  method per ext/pdo/pdo_dbh.c and pdo_stmt.c. Close the gap between stubs
  claims and runtime behavior (getColumnMeta IM001 mismatch). Test all 13 FETCH
  modes, all PARAM_* types, all ERRMODE/CASE/NULL handling modes, transaction
  isolation levels, and PDOException fields.
tags: [pdo, conformance, testing, fb3, v12.1]
priority: 1
---

# Spec: v12.1.0 PDO Definition Conformance

## Issue
#322 (this spec) — index for #328-#351 (24 test issues)

## Branch
`test/integration-conformance`

## Date
2026-07-07

## Status
Approved — execution in progress

---

## Intent

Verify that the `pdo_fbird` driver conforms to the PHP PDO Definition as specified
in the PHP source tree (`ext/pdo/pdo_dbh.c` lines 299-318, `pdo_stmt.c`). Every
mandatory method must be implemented and tested. Every optional method that the
stubs claim as supported must actually work at runtime. Standard PDO constants
(FETCH modes, PARAM types, ERRMODE, CASE, NULL handling) must all behave per the
PDO specification, matching what PHP developers expect from any PDO driver.

This milestone does NOT implement new features — it writes tests that surface gaps.
RED tests use `--SKIPIF--` so CI stays green until `feat/*` branches close the gaps.

---

## Background

### The PDO Definition

The base PDO extension (`ext/pdo/`) defines a contract all drivers must follow.
Derived from `php_pdo_driver.h` and method dispatch analysis in `pdo_dbh.c` /
`pdo_stmt.c`:

**Mandatory DBH methods** (called without NULL check — driver won't load without):
| Method | Source | Purpose |
|--------|--------|---------|
| `closer` | `pdo_dbh.c:1548` | Close database handle |
| `preparer` | `pdo_dbh.c:663` | Prepare SQL statement |
| `doer` | `pdo_dbh.c:1077` | Execute direct SQL (exec) |

**Mandatory-if-begin transaction methods**:
| Method | Source | Purpose |
|--------|--------|---------|
| `begin` | `pdo_dbh.c:703` | Begin transaction (NULL = unsupported) |
| `commit` | `pdo_dbh.c:733` | Commit (required if begin is set) |
| `rollback` | `pdo_dbh.c:757` | Rollback (required if begin is set) |

**Mandatory STMT methods** (called without NULL check):
| Method | Source | Purpose |
|--------|--------|---------|
| `executer` | `pdo_stmt.c:458` | Execute prepared statement |
| `fetcher` | `pdo_stmt.c:573` | Fetch next row |
| `describer` | `pdo_stmt.c:135` | Describe column (metadata) |
| `get_col` | `pdo_stmt.c:498` | Get column data |

**Optional methods** (NULL-checked — gracefully degrade):
`quoter`, `set_attribute`, `last_id`, `fetch_err`, `get_attribute`, `check_liveness`,
`get_driver_methods`, `in_transaction`, `persistent_shutdown`, `get_gc`, `scanner`,
`param_hook`, `get_column_meta`, `next_rowset`, `cursor_closer`

### Current gaps found in audit

| Gap | Issue | Severity |
|-----|-------|----------|
| `getColumnMeta()` returns IM001 but stubs claim "Supported" | #339 | Bug — stubs mismatch |
| `FBIRD_TXN_*` isolation levels documented but 0 tests | #334 | Untested feature |
| 9 of 13 FETCH modes untested | #342 | Untested standard |
| `PARAM_INT/STR/NULL/BOOL` never explicitly tested | #351 | Untested standard |
| `errorCode()` has 0 tests | #333 | Untested standard |
| Persistent connection lifecycle untested (issue #311 regression path) | #336 | Untested regression |

---

## User Stories / Acceptance Goals

### Goal 1: Mandatory methods work (GATE — blocks everything)

**Given** a running Firebird 3.0 server with `amicron-demo.fdb`
**When** a PHP dev calls `new PDO('fbird:...')`, `prepare()`, `exec()`, `fetch()`
**Then** all return expected types without errors

**Success Criteria**:
- [ ] #328 mandatory DBH methods test passes
- [ ] #329 mandatory transaction methods test passes
- [ ] #330 mandatory stmt methods test passes

### Goal 2: All 13 FETCH modes work

**Given** a PDOStatement with 3+ columns
**When** a dev calls `fetch()` with FETCH_ASSOC, FETCH_NUM, FETCH_BOTH, FETCH_OBJ, FETCH_LAZY, FETCH_BOUND, FETCH_COLUMN, FETCH_CLASS, FETCH_INTO, FETCH_FUNC, FETCH_NAMED, FETCH_KEY_PAIR, plus flags FETCH_GROUP/FETCH_UNIQUE/FETCH_CLASSTYPE/FETCH_PROPS_LATE
**Then** each mode returns data in the expected format

**Success Criteria**:
- [ ] #342 all 13 FETCH modes tested (RED tests use --SKIPIF-- for unsupported modes)

### Goal 3: getColumnMeta mismatch resolved

**Given** the stubs claim getColumnMeta is "Supported"
**When** a dev calls `$stmt->getColumnMeta(0)`
**Then** either (a) it returns metadata array, or (b) stubs are corrected to "Not yet implemented"

**Success Criteria**:
- [ ] #339 bug resolved — either implemented or stubs corrected

### Goal 4: Transaction isolation levels testable

**Given** `PDO::FBIRD_TXN_READ_COMMITTED` / `_REPEATABLE_READ` / `_SERIALIZABLE` constants exist
**When** a dev calls `setAttribute(PDO::FBIRD_ATTR_TRANSACTION_ISOLATION_LEVEL, ...)`
**Then** the isolation level is applied and queryable via `getAttribute()`

**Success Criteria**:
- [ ] #334 FBIRD_TXN_* isolation levels tested and passing

### Goal 5: All PARAM_* types bind correctly

**Given** a prepared statement with typed parameters
**When** a dev calls `bindParam()` / `bindValue()` with PARAM_NULL, PARAM_INT, PARAM_STR, PARAM_LOB, PARAM_BOOL, PARAM_INPUT_OUTPUT
**Then** values are correctly typed and stored

**Success Criteria**:
- [ ] #351 all standard PARAM_* types tested

---

## Out of Scope

- Implementation of any gap (those are M9 `feat/*` branches)
- FB 4.0+/5.0+/6.0 features (M8 stretch, v13.0.0)
- Procedural `fbird_*` API gaps (M2 milestone)
- Multiple active result sets (#435, deferred to v13.0)

---

## Constitution Alignment

| Article | Requirement | How Met |
|---------|-------------|---------|
| II: Test-First | Tests before implementation | All 24 issues are test files; gap implementations are separate feat/* branches |
| III: Memory Safety | ASan/UBSan clean | Tests run in ASan container (#318 coverage bump) |
| VII: Coverage Gate | Coverage >= 60% | #318 bumps threshold from 54% to 60% |

---

## Dependencies

- **Requires**: #320 (conformance skipif.inc), #316 (pdo-conformance CI job)
- **Blocks**: M3 (Doctrine tests need PDO conformance baseline), M6 (docs reflect actual PDO state)

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| #339 getColumnMeta: implementing it is non-trivial (needs describe_col in driver) | High | Medium | Ship as RED test first; defer implementation to feat/v12.1.1 branch |
| #336 persistent conn: SIGSEGV regression from #311 may still exist in PDO path | Medium | High | Test with ASan; if fails, block v12.1.0 release |
| #334 FBIRD_TXN_*: constants may exist but setAttribute may silently fail | Medium | Medium | Test getAttribute round-trip; if fails, file bug issue |

---

## Open Questions

- [ ] Should `getColumnMeta()` be implemented (matching bundled `pdo_firebird` which HAS it) or should stubs be corrected? **Recommended**: implement in feat/v12.1.1 (non-breaking patch), correct stubs immediately as stopgap.
- [ ] Does `PARAM_INPUT_OUTPUT` have any meaning for Firebird (engine supports in/out params via EXECUTE PROCEDURE RETURNING)? **Recommended**: document as not supported, matching bundled pdo_firebird.

---

## Issue Index

| Issue | Title | Priority |
|-------|-------|----------|
| #328 | test: PDO Definition mandatory DBH methods | P0 |
| #329 | test: PDO Definition mandatory transaction methods | P0 |
| #330 | test: PDO Definition mandatory stmt methods | P0 |
| #331 | test: PDO Definition optional quoter | P1 |
| #332 | test: PDO Definition optional last_id | P1 |
| #333 | test: PDO Definition optional fetch_err | P1 |
| #334 | test: PDO Definition optional get/set_attribute (incl FBIRD_TXN_*) | P1 |
| #335 | test: PDO Definition optional check_liveness | P1 |
| #336 | test: PDO Definition optional persistent connections | P1 |
| #337 | test: PDO Definition optional get_driver_methods | P2 |
| #338 | test: PDO Definition optional param_hook | P2 |
| #339 | **bug**: PDO getColumnMeta returns IM001 but stubs claim supported | P0 |
| #340 | test: PDO Definition optional next_rowset (N/A) | P2 |
| #341 | test: PDO Definition optional cursor_closer | P2 |
| #342 | test: PDO Definition all 13 FETCH modes | P1 |
| #343 | test: PDO Definition all 6 FETCH_ORI_* (FB5+) | P2 |
| #344 | test: PDO Definition standard cursor attrs | P2 |
| #345 | test: PDO Definition all 3 ERRMODE modes | P1 |
| #346 | test: PDO Definition case folding | P1 |
| #347 | test: PDO Definition null handling | P1 |
| #348 | test: PDO Definition PDOException fields | P1 |
| #349 | test: PDO Definition driver registration | P2 |
| #350 | test: PDO Definition driver API version | P2 |
| #351 | test: PDO Definition all standard PARAM_* types | P1 |

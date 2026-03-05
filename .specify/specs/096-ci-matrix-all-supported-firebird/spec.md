# Feature Specification: CI Matrix — All Supported Firebird Versions

**Feature Branch**: `feat/096-ci-matrix-all-supported-firebird`
**Created**: 2026-03-05
**Status**: Draft

## Context

php-firebird builds against the Firebird client library (`libfbclient.so`). The client
library version determines which `FB_API_VER` guards are compiled in:

- `FB_API_VER = 30` — Firebird 3.0 headers (no batch ops, no time zones, no INT128)
- `FB_API_VER = 40` — Firebird 4.0 headers (batch ops, time zones, DECFLOAT, INT128)
- `FB_API_VER = 50` — Firebird 5.0 headers (scrollable cursors, inline ODS upgrade)

A FB5 client can connect to a FB3 server (wire protocol negotiation), but building
against FB3 headers means `#if FB_API_VER >= 40` code is never compiled or tested.
Therefore each client version must be tested independently.

**Firebird support status (March 2026)**:
- Firebird 3.0 — ACTIVE (3.0.14 planned Q3 2026)
- Firebird 4.0 — ACTIVE (4.0.7 planned Q1 2026)
- Firebird 5.0 — ACTIVE (5.0.3/5.0.4 current)
- Firebird 2.5 — EOL June 2019 (removed in php-firebird v7.2.0)

**Current CI gap**: The CI matrix covers PHP 8.2/8.4 × FB 3.0/5.0 only.
- Firebird 4.0 is actively supported but never tested in CI
- PHP 8.3 is required by constitution Article V but absent from CI
- The CI already has FB 4.0 download logic (version 4.0.6) — it just has no matrix entry

---

## User Scenarios & Testing

### User Story 1 - Firebird 4.0 users get CI coverage (Priority: P1)

**Why this priority**: FB4 is actively supported, has unique features (`#if FB_API_VER >= 40`
guards batch ops, time zones, DECFLOAT, INT128). Without FB4 in CI, regressions in
FB4-specific code paths are invisible.

**Independent Test**: Run CI on any PR — all 9 matrix jobs must pass.

**Acceptance Scenarios**:
1. **Given** a push to `satware-main`, **When** CI runs, **Then** PHP 8.2/FB4, PHP 8.3/FB4,
   PHP 8.4/FB4 jobs all appear in the matrix and pass.
2. **Given** a code change that breaks FB4-specific code, **When** CI runs, **Then** the
   PHP x/FB4 jobs fail and block the PR.

### User Story 2 - PHP 8.3 users get CI coverage (Priority: P1)

**Why this priority**: Constitution Article V mandates PHP 8.2, 8.3, 8.4. PHP 8.3 is
currently absent from CI despite `Dockerfile-8.3` existing.

**Independent Test**: PHP 8.3 job appears in CI matrix and passes all PHPT tests.

**Acceptance Scenarios**:
1. **Given** a push to `satware-main`, **When** CI runs, **Then** at least one PHP 8.3
   matrix entry appears and passes.
2. **Given** a PHP 8.3-specific compilation warning, **When** CI runs, **Then** the
   PHP 8.3 job surfaces it.

### User Story 3 - Documentation reflects actual tested matrix (Priority: P2)

**Why this priority**: `AGENTS.md`, `project-brief.md`, and `constitution.md` currently
describe the matrix incorrectly (missing FB4 and PHP 8.3).

**Acceptance Scenarios**:
1. **Given** the updated CI, **When** a contributor reads `AGENTS.md`, **Then** the
   documented matrix matches the actual CI matrix.

---

## Requirements

### Functional Requirements

- **FR-001**: CI MUST include Firebird 4.0 as a server version in the matrix
- **FR-002**: CI MUST include PHP 8.3 as a PHP version in the matrix
- **FR-003**: The FB 4.0 client download in CI MUST use a current stable release (≥4.0.6)
- **FR-004**: All new matrix entries MUST use the same `db-path` convention as existing entries
- **FR-005**: The `Install Firebird client library` step MUST handle `fb-client-version: '4.0'`
  (already present in CI — verify it is correct)

### Key Entities

- **CI matrix entry**: `{php-version, firebird-version, firebird-image, fb-client-version, db-path}`
- **FB_API_VER**: Compile-time macro from `ibase.h` — determines which feature guards compile
- **Firebird client tarball**: Downloaded from GitHub releases during CI; version pinned per matrix entry

### Non-Functional Requirements

- **NFR-001**: CI total job count MUST NOT exceed 12 (to stay within GitHub Actions free tier limits)
- **NFR-002**: New matrix entries MUST NOT require new Dockerfiles (reuse existing PHP images)
- **NFR-003**: CI runtime per job MUST remain under 15 minutes

---

## Target Matrix (9 combinations)

| PHP | FB 3.0 | FB 4.0 | FB 5.0 |
|-----|--------|--------|--------|
| 8.2 (min) | ✅ existing | ✅ **new** | ✅ existing |
| 8.3 | - | ✅ **new** | - |
| 8.4 (current) | ✅ existing | ✅ **new** | ✅ existing |

Rationale for PHP 8.3 × FB4 only (not full row):
- PHP 8.3 is an intermediate version; full coverage is PHP 8.2 (min) and 8.4 (current)
- One PHP 8.3 entry proves compilation and test pass; FB4 is the most feature-rich middle ground
- Keeps total jobs at 7 (not 9) — within NFR-001

**Final matrix: 7 combinations**

| PHP | FB 3.0 | FB 4.0 | FB 5.0 |
|-----|--------|--------|--------|
| 8.2 | ✅ | ✅ new | ✅ |
| 8.3 | - | ✅ new | - |
| 8.4 | ✅ | ✅ new | ✅ |

---

## Success Criteria

- **SC-001**: CI matrix contains exactly 7 entries (was 4)
- **SC-002**: All 7 CI jobs pass on `satware-main` after implementation
- **SC-003**: `AGENTS.md` matrix table updated to reflect 7 combinations
- **SC-004**: `constitution.md` Article V updated to list PHP 8.2, 8.3, 8.4 and FB 3.0, 4.0, 5.0
- **SC-005**: `project-brief.md` CI matrix section updated

---

## Out of Scope

- PHP 8.5 CI entry (separate issue — 8.5 is not yet stable)
- Firebird 4.0 Docker service in `docker-compose.yml` (already present as `firebird40`)
- Windows CI matrix changes
- Any C code changes (all `#if FB_API_VER >= 40` guards already exist)
- Changing the FB 4.0 client version pinned in CI (4.0.6 is current stable)

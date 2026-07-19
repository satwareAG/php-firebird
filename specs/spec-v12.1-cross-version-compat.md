---
description: >-
  v12.1.0 Cross-Version Compatibility: verify FB5 client connects to FB
  2.5/3.0/4.0/5.0 servers (backward compat), test DataTypeCompatibility
  modes, verify ODS version handling.
tags: [cross-version, fb2.5-backcompat, fb3, testing, v12.1]
priority: 1
---

# Spec: v12.1.0 Cross-Version Compat

## Issue
#326 (this spec) - index for #313, #404-#409

## Branch
`test/integration-conformance`

## Date
2026-07-07

## Status
COMPLETE — 8 issues closed (#326, #313, #404-#409). FB5 client to FB 2.5/3/4/5 verified.

---

## Intent

Verify that the Firebird 5.0 client library (used by php-firebird) can
connect to Firebird servers of all supported versions (2.5, 3.0, 4.0, 5.0).
Test DataTypeCompatibility modes for type coercion. Verify ODS version
handling across server versions.

---

## ODS Version Table

| ODS | Firebird Version |
|-----|-----------------|
| 11.x | FB 2.5 |
| 12.0 | FB 3.0 |
| 13.0 | FB 4.0 |
| 13.1 | FB 5.0 |
| 14.0 | FB 6.0 |

The FB5 client can attach to any ODS 11.x+ database.

---

## Wire Protocol Matrix

| Client | Server | Protocol | Status |
|--------|--------|----------|--------|
| FB 5.0 client | FB 2.5 server | Protocol 10-12 | Supported (legacy auth) |
| FB 5.0 client | FB 3.0 server | Protocol 13 | Supported (Srp auth) |
| FB 5.0 client | FB 4.0 server | Protocol 14-16 | Supported |
| FB 5.0 client | FB 5.0 server | Protocol 17-18 | Supported |
| FB 3.0 client | FB 6.0 server | - | Not supported (new auth plugins) |

---

## DataTypeCompatibility

FB 4.0+ introduces new SQL types (DECFLOAT, INT128, TIME/TIMESTAMP WITH TIME ZONE).
DataTypeCompatibility controls how these types are presented to older clients:

| Mode | Coercion |
|------|----------|
| Default (FB4+) | Native types returned as-is |
| `=3.0` | DECFLOAT→DOUBLE, INT128→BIGINT, TZ→TIME/TIMESTAMP |
| `=2.5` | Same as 3.0 + BOOLEAN→CHAR(5) |

SET BIND SQL statements provide per-session control:
```sql
SET BIND OF DECFLOAT TO DOUBLE PRECISION;
SET BIND OF INT128 TO BIGINT;
SET BIND OF TIME ZONE TO LEGACY;
```

---

## Out of Scope

- FB 6.0 server (no Docker image available, #408 deferred to M8)
- FB 2.5 SQL features (legacy, not target for amicron-platform)
- Performance benchmarking across versions

---

## Issue Index

| Issue | Title | Status |
|-------|-------|--------|
| #326 | spec: write this file | Closed |
| #313 | ci: add firebird25 Docker service | Closed |
| #404 | ci: add cross-version compatibility test matrix | Closed |
| #405 | test: FB5 client to FB 2.5 server backward compat | Closed |
| #406 | test: DataTypeCompatibility=2.5 mode | Closed |
| #407 | test: DataTypeCompatibility=3.0 mode | Closed |
| #408 | test: REVERSE - FB3 client to FB6 server | Deferred (M8) |
| #409 | test: ODS minor-version compatibility | Closed |

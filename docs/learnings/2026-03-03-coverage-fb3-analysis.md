---
description: >-
  Lessons learned pushing php-firebird code coverage from 61.8% to 65.2%
  on the FB3 OO-API baseline (php84-fb3-dev container, 7802 measured lines).
tags:
  - coverage
  - firebird
  - php-extension
  - c
  - lcov
last_updated: '2026-03-03'
---

# 2026-03-03: Coverage Push — FB3 OO-API Baseline Analysis

## Problem

Coverage gate target: ≥80% overall lines on FB3 build.
Starting point: 61.8% (4,825/7,802). Need 1,416 more covered lines.

## Key Findings

### 1. FB5 container CANNOT be used for coverage measurement
Running `php85-fb5-dev` activates `#if FB_API_VER >= 40` blocks, raising total
lines from 7,802 → 9,155. Even if MORE lines are covered, the % DROPS.
The authoritative container is always `php84-fb3-dev`.

### 2. `_php_fbird_safe_copy_sqlvar_data()` is dead code on FB3+
`fbird_query_bind.c` lines 36–220 are ONLY reachable via the Firebird 2.5
legacy `isc_dsql` API. On FB3+, the OO API path is always taken.
These ~87 lines cannot be covered from PHP tests on FB3 without a
Firebird 2.5 server. Mark with `LCOV_EXCL_START/STOP` if needed.

### 3. SET TRANSACTION via fbird_query requires transaction-first calling convention
```php
// WRONG — passes $tx as bind parameter:
fbird_query($dbh, 'INSERT ...', $tx);
// CORRECT — $tx is the first argument (connection/transaction):
fbird_query($tx, 'INSERT ...');
```

### 4. After explicit SET TRANSACTION commit, use fbird_commit($dbh) before SELECT
The default transaction on `$dbh` may still be open (snapshot isolation).
It won't see rows committed by an explicitly-created SET TRANSACTION resource
until the default transaction is closed and a new one started.

### 5. firebird_utils.cpp batch API (281-line gap) is FB4+ only
Cannot be covered by `php84-fb3-dev`. Would need `php84-fb4-dev` or
`php84-fb5-dev` BUT container selection affects total line count (see #1).
Resolution: Add separate FB4+ coverage job to CI; report separately.

### 6. fbird_datetime.c jumped to 91% (was listed as 32.7% in task)
Either previous test runs already covered most paths, or the task description
was based on an older lcov snapshot. Always re-run coverage.sh before planning.

## Solution Pattern

```phpt
--TEST--
Coverage target: specific function name + file:line range
--SKIPIF--
<?php include __DIR__ . '/../skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/../firebird.inc';
// ...
?>
--EXPECT--
ok
```

Use `skipif.inc` (not inline skip) — it checks extension loaded + server available.

## Lesson

**Always regenerate lcov before planning** — cached coverage data goes stale
after any test changes. The 65.2% baseline after this session is the truth.


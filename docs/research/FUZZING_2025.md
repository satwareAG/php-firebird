# State-of-the-Art Database Fuzzing (2025)

## Overview

This document summarizes research into modern database fuzzing techniques and their application to the PHP Firebird extension. The goal is to move beyond basic random input generation to coverage-guided, grammar-aware, and edge-case-focused fuzzing that can detect subtle memory safety issues and logic bugs.

## 1. Modern SQL Fuzzing Techniques (2025)

Recent advancements in database fuzzing, exemplified by tools like **BuzzHouse** (ClickHouse fuzzer) and **SQLancer**, emphasize the following patterns:

### 1.1 Catalog-Aware Generation
Instead of generating random strings for table/column names, modern fuzzers query the database catalog (metadata) to discover valid schema objects. This significantly increases the probability of generating valid SQL queries that exercise the execution engine rather than just the parser.

**Application to Firebird:**
- Query `RDB$RELATIONS` and `RDB$RELATION_FIELDS` to discover tables and columns.
- Use discovered metadata to construct valid `INSERT`, `UPDATE`, and `SELECT` statements.

### 1.2 Grammar-Aware Mutation
Using a formal grammar (e.g., BNF) to generate complex SQL structures including:
- Nested subqueries
- `JOIN` operations with complex conditions
- `GROUP BY` and `HAVING` clauses
- Window functions (Firebird 3.0+)

### 1.3 Coverage-Guided Feedback
Integrating with sanitizers (ASan, UBSan) and coverage tools (gcov, LLVM cov) to guide the fuzzer towards unexplored code paths. While full coverage-guided fuzzing (like AFL++) requires binary instrumentation, we can approximate this by tracking API function usage and ensuring all `fbird_*` functions are exercised.

## 2. Firebird-Specific Edge Cases

Research into the Firebird bug tracker and release notes highlights specific areas prone to instability:

### 2.1 BLOB Handling
BLOBs are historically a source of memory corruption and logic errors.
- **Zero-Length Segments**: Multi-byte character sets could cause endless loops when processing zero-length segments (CORE-1063).
- **Large BLOBs**: Handling BLOBs larger than 100MB (or exceeding RAM limits) can trigger stability issues.
- **Segmented Writes**: Writing BLOBs in many small segments vs. single large chunks exercises different code paths in the client library.
- **Post-Commit Access**: Accessing a BLOB handle after the transaction has committed should fail gracefully, but has historically caused crashes or use-after-free issues.

### 2.2 Transaction Chaos
Firebird's multi-generational architecture relies heavily on correct transaction handling.
- **Random Commit/Rollback**: Rapidly cycling transactions stresses the garbage collection and versioning engine.
- **Savepoints**: Creating and rolling back to savepoints (`fbird_savepoint`) adds complexity to the transaction state machine.
- **Retain Context**: Using `fbird_commit_ret` and `fbird_rollback_ret` keeps the transaction context open, which interacts differently with cursors and BLOB handles.

### 2.3 Boundary Conditions
- **INT128**: Firebird 4.0+ supports 128-bit integers. PHP handles these as strings or GMP objects. Fuzzing must ensure correct binding and retrieval.
- **NUMERIC Precision**: `NUMERIC(38, x)` types require precise handling to avoid overflow or truncation.
- **Timestamp Extremes**: Dates near the limits (e.g., year 0001, 9999) can expose overflow bugs in date calculation logic.

## 3. Implementation Strategy

Based on this research, the enhanced fuzzer (`tests/fuzzer.php`) implements:

1.  **Modular Architecture**: Separate functions for each operation type (INSERT, UPDATE, BLOB, etc.) allowing weighted random selection.
2.  **State Tracking**: Maintains knowledge of active transactions, prepared statements, and BLOB handles to generate valid sequence-dependent operations.
3.  **Sanitizer Integration**: Designed to run inside the `php83-asan` container to catch memory safety violations immediately.
4.  **AI-Optimized Reporting**: Outputs structured JSON reports that group errors and provide coverage metrics, facilitating automated analysis by AI agents.

## 4. References

1.  **BuzzHouse**: ClickHouse Fuzzer (https://github.com/ClickHouse/BuzzHouse)
2.  **SQLancer**: Automated Testing to Detect Logic Bugs in DBMS (https://github.com/sqlancer/sqlancer)
3.  **Firebird Tracker**: CORE-1063, CORE-2608, CORE-1610
4.  **PHP Firebird Extension**: Existing test suite and bug reports

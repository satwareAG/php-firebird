# PHP Extension Bug Detection: 2025 Best Practices

## Overview

This document outlines modern best practices for detecting memory and logic bugs in PHP extensions (C/C++ using Zend Engine), based on research into 2024-2025 trends in database fuzzing and low-level systems programming.

## 1. Memory Safety (Zend Engine & C/C++)

PHP extensions operate in a managed memory environment (Zend Memory Manager) but interact with raw system memory, making them prone to unique classes of bugs.

### Key Tools & Techniques

| Tool | Purpose | Integration Strategy |
|------|---------|----------------------|
| **AddressSanitizer (ASan)** | Detects buffer overflows, use-after-free, double-free. | Compile PHP with `-fsanitize=address`. Use `USE_ZEND_ALLOC=0` to bypass Zend MM for ASan visibility. |
| **Valgrind (Memcheck)** | Detects memory leaks, uninitialized memory usage. | Run `make test` with `VALGRIND=1`. Essential for finding leaks in persistent resources. |
| **ThreadSanitizer (TSan)** | Detects data races in multi-threaded environments (ZTS). | Crucial for ZTS builds of PHP 8.x+. |
| **Clang Static Analyzer** | Static path analysis for null dereferences and logic errors. | Integrate into CI pipeline via `scan-build`. |

### 2025 Specifics for PHP Extensions

*   **Zend Assertions**: Ensure debug builds (`--enable-debug`) are used during fuzzing to trigger internal `ZEND_ASSERT` failures, which often precede memory corruption.
*   **Reference Counting**: Common source of bugs. Fuzzers should specifically target object lifetime edges (e.g., passing objects, returning objects, exceptions during object construction).

## 2. Logic Bug Detection (Database Connectors)

Detecting that an extension *crashes* is "easy" (segfault). Detecting that it returns *wrong data* or ends up in an *inconsistent state* is harder.

### Modern Techniques (SQLancer & Beyond)

Research from 2024-2025 highlights **Differential Testing** and **Oracle-Based Fuzzing** as the gold standard.

#### A. Ternary Logic Partitioning (TLP)
Used by SQLancer. The core idea is to construct a query that *must* return a specific set of rows based on logic, regardless of the execution plan.
*   **Concept**: `SELECT * FROM t WHERE p` + `SELECT * FROM t WHERE NOT p` + `SELECT * FROM t WHERE p IS NULL` should equal `SELECT * FROM t`.
*   **Application to PHP Driver**: We can generate a random `WHERE` clause, execute the three partitioned queries via the PHP extension, and verify the counts match the total count. If they don't, the driver (or the DB) has a logic bug in handling types/values.

#### B. Non-Optimizing Reference Engine Construction (NoREC)
*   **Concept**: Compare a query that might be optimized (using indexes, complex joins) against a "ground truth" query that forces a full table scan or simple evaluation.
*   **Application**: Execute a complex query via `fbird_query` and compare results against a PHP-side implementation of the same logic (fetching all rows and filtering in PHP).

#### C. State-Aware Connector Fuzzing
Recent research (2024) emphasizes fuzzing the *connector state machine*.
*   **Focus**: Prepared statements lifecycle, transaction isolation levels, and cursor states.
*   **Technique**: Generate sequences of operations that specifically stress state transitions (e.g., `prepare` -> `execute` -> `commit` -> `fetch` -> `error`).

## 3. Implementation Plan for `php-firebird`

### Phase 1: Enhanced Memory Fuzzing (Completed)
*   ASan integration.
*   Basic operation fuzzing (connect, query, blob).

### Phase 2: Logic Oracle Implementation (Next Steps)
*   **TLP Oracle**: Implement a `LogicOps` class in the fuzzer.
    *   Generate a random table with random data.
    *   Generate a random predicate `P`.
    *   Run `SELECT COUNT(*) WHERE P`, `WHERE NOT P`, `WHERE P IS NULL`.
    *   Assert sum equals total rows.
*   **PHP-Side Verification**:
    *   Fetch data as `INT`, `STRING`, `FLOAT`.
    *   Verify PHP type coercion matches Firebird expectations.

### Phase 3: State Machine Fuzzing
*   Model the Firebird client state (Disconnected, Connected, TransactionActive, StatementPrepared, CursorOpen).
*   Generate operation sequences that attempt invalid transitions or edge cases (e.g., double commit, fetch after close).

## References
1.  *SQLxDiff: Testing Logic Bugs in Database Systems*, 2024.
2.  *SQLancer: Automated Testing to Find Logic Bugs in Database Systems*.
3.  *LLM-Guided Fuzzing for Database Connectors*, 2024.

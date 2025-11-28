# php-firebird Extension Improvement Roadmap

This document outlines the plan to enhance the developer experience, reliability, and modern PHP compatibility of the `php-firebird` extension. The goal is to transform the extension from "working" to "robust and developer-friendly", aligning it with modern standards expected by frameworks like Doctrine and Rails.

## Objectives

*   **Modernize Error Handling:** Transition from Warnings to Exceptions for better control and standardized error management.
*   **Clean Loop Semantics:** Ensure `fetch` functions behave predictably at the end of result sets (no extraneous warnings).
*   **Efficiency:** Implement prepared statement reuse to reduce overhead.
*   **Resource Management:** Improve resource lifecycle management, particularly for transactions (`commit_retaining`).
*   **Feature Parity:** Add support for missing Firebird features like `RETURNING` clause values and proper BLOB streaming.
*   **Consistency:** Ensure all functions feature consistent `fbird_*` aliases.

## Roadmap Phases

### Phase 1: Core Interaction & Error Handling (COMPLETED)

This phase addresses the most visible and annoying issues for developers: unpredictable warnings during iteration and archaic error handling.

*   **Recommendation #1: Clean Fetch Loop**
    *   **Issue:** `fbird_fetch_*` functions currently emit Warnings when they reach the end of a result set or encounter EOF in certain conditions. This forces developers to use silence operators (`@`) or complex error handlers.
    *   **Goal:** `fetch` should simply return `false` on EOF/end-of-results without emitting a PHP Warning.
    *   **Status:** **Verified & Clean.** Investigation showed standard `fetch` loop operates cleanly without warnings on EOF. No changes were required, but the behavior is now verified by tests.

*   **Recommendation #6: Exception-Based Error Handling**
    *   **Issue:** The extension primarily uses `php_error_docref` to emit Warnings. Modern PHP code expects Exceptions (`FirebirdException` or `PDOException` style) to handle database errors gracefully.
    *   **Goal:** Introduce a mechanism to throw Exceptions instead of Warnings. This might be an opt-in configuration initially to preserve backward compatibility.
    *   **Implementation:**
        *   Added INI setting `ibase.enable_exceptions` (default: 0).
        *   Registered `Firebird\Exception` class.
        *   Modified internal error handlers to throw `Firebird\Exception` when enabled.
    *   **Status:** **Completed.**

### Phase 2: Statement Stability & Lifecycle (COMPLETED)

This phase focuses on performance and correctness of long-running scripts or complex transactional logic.

*   **Recommendation #2: Prepared Statement Reuse**
    *   **Issue:** `fbird_execute` currently might be re-preparing statements unnecessarily or not leveraging the full potential of Firebird's prepared statement model, especially when called repeatedly.
    *   **Goal:** Ensure `fbird_prepare` returned resources can be executed multiple times efficiently without leakage or re-preparation overhead.
    *   **Status:** **Verified.** Tests confirmed that prepared statements can be reused multiple times with different parameters without error.

*   **Recommendation #3: `commit_retaining` Resource Lifecycle**
    *   **Issue:** `fbird_commit_ret` (Commit Retaining) keeps the transaction context open on the server but might be invalidating PHP resources (`$trans`) prematurely or incorrectly, confusing the extension's internal state tracking.
    *   **Goal:** Ensure PHP resource handles remain valid and usable after a `commit_retaining` call, reflecting the actual Firebird transaction state.
    *   **Status:** **Verified.** Tests confirmed `commit_ret` keeps the transaction handle valid and usable for subsequent queries.

### Phase 3: Advanced Features & Modernization (COMPLETED)

This phase enables powerful Firebird specific features.

*   **Recommendation #4: BLOB Streaming**
    *   **Issue:** BLOBs are often loaded fully into memory.
    *   **Goal:** Expose BLOBs as PHP Streams, allowing memory-efficient handling of large files.
    *   **Status:** **Completed.** Implemented `ibase_blob_create_stream()` and `ibase_blob_open_stream()`.

*   **Recommendation #5: `RETURNING` / Insert IDs**
    *   **Issue:** Getting the ID of an inserted row (especially with triggers/generators) is non-standardized.
    *   **Goal:** reliably support the `RETURNING` clause and expose it via a simple API (e.g. `fbird_insert_id()` or by returning the values from `execute`).
    *   **Status:** **Verified.** Tests confirmed that `INSERT ... RETURNING` works out-of-the-box with `ibase_query()`/`ibase_execute()` returning a result set containing the returned values.

*   **Recommendation #7: Function Aliases**
    *   **Issue:** Inconsistency between `ibase_*` and `fbird_*` function names.
    *   **Goal:** Ensure all functions have `fbird_*` aliases.
    *   **Status:** **Completed.** Added missing aliases for blob stream functions.

## Development Log

*   **2025-11-28:** Roadmap initialized. Starting analysis of Recommendation #1 (Fetch Warnings) and #6 (Exceptions).
*   **2025-11-28:** Phase 1 Complete. Exception handling implemented (`ibase.enable_exceptions`). Fetch loop verified to be clean.
*   **2025-11-28:** Phase 2 Complete. Statement reuse and commit retaining lifecycle verified via reproduction scripts.
*   **2025-11-28:** Phase 3 Complete. BLOB Streaming implemented. `RETURNING` support verified. Aliases updated.

# Coverage Expansion Plan: "Every Line of Code"

## Current Status (Dec 2025)
- **Line Coverage**: 56.6%
- **Function Coverage**: 83.2%
- **Critical Gaps**:
  - `fbird_query_array.c` (32%): Array binding and result handling.
  - `fbird_query_bind.c` (40%): Complex parameter binding scenarios.
  - `fbird_inspection.c` (44%): Database inspection tools.
  - `fbird_events.c` (44%): Event handling logic.
  - `firebird_utils.cpp` (46%): Core C++ utility functions.

## Objective
Achieve >90% line coverage and 100% function coverage, ensuring all paths are exercised under ASan and UBSan to detect hidden memory errors.

## Strategy

### 1. Targeted Test Implementation
We will create specific PHPT tests for the low-coverage areas:

#### A. Query Array Binding (`fbird_query_array.c`)
- **Gap**: Handling of complex array structures in bindings.
- **Action**: Create `tests/coverage/query_array_complex.phpt` testing nested arrays, nulls in arrays, and invalid array structures.

#### B. Parameter Binding Edge Cases (`fbird_query_bind.c`)
- **Gap**: Edge cases in type conversion and buffer allocation.
- **Action**: Create `tests/coverage/bind_edge_cases.phpt` testing boundary values (INT64_MAX, empty strings, huge blobs) and type mismatches.

#### C. Event Handling (`fbird_events.c`)
- **Gap**: Error conditions in event registration and callback execution.
- **Action**: Create `tests/coverage/events_error_handling.phpt` simulating network interruptions during event waiting and invalid callback signatures.

#### D. Inspection Tools (`fbird_inspection.c`)
- **Gap**: System table queries and metadata extraction.
- **Action**: Create `tests/coverage/inspection_deep.phpt` querying all available metadata fields and handling permission errors.

### 2. Fuzz Testing Integration
- Implement a basic fuzzer in PHP that generates random SQL queries and parameter sets to hit unexpected paths in `firebird_utils.cpp`.
- Run this fuzzer under ASan for 1 hour.

### 3. Sanitizer Matrix
- Ensure all new tests are added to the `scripts/analysis/sanitizers.sh` suite.
- Run the full suite with:
  - ASan (AddressSanitizer)
  - UBSan (UndefinedBehaviorSanitizer)
  - LSan (LeakSanitizer)
  - Valgrind (Memcheck)

## Execution Plan

1. **Phase 1 (Completed)**: Implement tests for `fbird_query_array.c` and `fbird_query_bind.c`.
2. **Phase 2 (Completed)**: Implement tests for `fbird_events.c` and `fbird_inspection.c`.
3. **Phase 3**: Develop and run the SQL fuzzer.
4. **Phase 4**: Final coverage report generation and verification.

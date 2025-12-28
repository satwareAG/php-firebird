# QA Summary

## Recent Achievements (Dec 2025)

### 1. Sanitizer Pipeline Stability
- **Docker Permissions**: Fixed root-owned artifact issues in `scripts/qa.sh` using `chown` and `make clean`.
- **UBSan Compatibility**: Updated `scripts/analysis/sanitizers.sh` to use GCC-compatible flags (`-fsanitize=undefined`), resolving build failures.
- **Documentation**: Created `docs/development/SANITIZER_STRATEGY.md` detailing the multi-layered sanitizer approach.

### 2. Critical Bug Fixes
- **Memory Leak & UAF in `fbird_query`**: Identified and fixed a complex Use-After-Free (UAF) and Memory Leak issue in `fbird_query` and `fbird_execute`.
  - **Issue**: `zend_get_parameters_array_ex` creates copies of arguments. `convert_to_string` in `_php_fbird_bind` modifies these copies (allocating new strings).
  - **Fix**: Added `zval_ptr_dtor` loop for arguments passed to `_php_fbird_bind` (starting from `bind_start` index) to free allocated strings/arrays, while avoiding double-free on resources (which are not addref-ed by `zend_get_parameters_array_ex` in a way that survives full dtor).
  - **Verification**: Verified with ASan (AddressSanitizer) and LSan (LeakSanitizer).

### 3. Coverage Expansion
- **New Tests**:
  - `tests/coverage/query_array_complex.phpt`: Covers complex array binding and error handling in `fbird_query_array.c`.
  - `tests/coverage/bind_edge_cases.phpt`: Covers edge cases (INT64 limits, empty strings, large blobs) in `fbird_query_bind.c`.
  - `tests/coverage/events_error_handling.phpt`: Covers error paths in `fbird_events.c` (invalid resources, argument counts).
  - `tests/coverage/inspection_deep.phpt`: Covers error paths in `fbird_inspection.c` (non-existent tables, invalid resources).
- **Status**:
  - Line Coverage: 56.7% (up from 56.6%)
  - Function Coverage: 83.2%
  - `fbird_query_exec.c` coverage improved to 66%.

### 4. Research & Architecture
- **Advanced DB Clients**: Documented best practices for 2025 (PHP 8.4+, strict types, RAII C++) in `docs/research/ADVANCED_DB_CLIENTS_2025.md`.
- **Coverage Plan**: Created `docs/planning/COVERAGE_EXPANSION_PLAN.md` outlining the strategy to reach >90% coverage.

### 5. Fuzzing Infrastructure
- **Integration**: Integrated fuzzing into the main QA workflow (`scripts/qa.sh --mode full`).
- **Bug Detection**: Fuzzer successfully detected a critical Heap Use-After-Free (UAF) in `fbird_pconnect`.
- **Architecture**: Implemented a modular fuzzing framework in `fuzz/` with SARIF reporting and ASan integration.

## Next Steps
1. **Fix UAF Bug**: Resolve the Heap Use-After-Free issue in persistent connections.
2. **Modernization**: Begin refactoring internal C++ classes to use RAII (smart pointers) to prevent future leaks.

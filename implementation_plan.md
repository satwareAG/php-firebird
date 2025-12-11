# Implementation Plan

[Overview]
Fix all static analysis warnings and errors to achieve maximum code quality and stability in the PHP Firebird extension.

This implementation addresses 4 compilation errors in fbird_udf.c, approximately 50 clang-tidy warnings across 10 C source files, 2 cppcheck warnings, and 2 failing unit tests. The issues fall into these categories: macro argument errors, missing switch default cases, readability violations (else-after-return), pointer sign mismatches, uninitialized variable usage, tautological comparisons, and loop variable type narrowing. Fixing these issues ensures the extension compiles cleanly, passes static analysis, and maintains runtime stability.

[Types]
No new types are introduced; changes affect existing C code only.

This plan modifies existing C/C++ code to fix static analysis issues. No new type definitions, interfaces, or data structures are required. The changes involve correcting macro usage, adding defensive code (default cases), refactoring control flow, and fixing type mismatches in existing code.

[Files]
Nine C source files require modifications to address all identified issues.

**Files to be modified:**

1. **fbird_udf.c** - CRITICAL: 4 compilation errors + multiple warnings
   - Fix ZVAL_STRINGL macro argument count (lines 277, 282)
   - Fix pointer sign conversions (lines 157, 235)
   - Fix implicit conversion constant (line 157)
   - Fix switch-bool warning (line 161)
   - Add missing switch default cases (lines 217, 304)

2. **fbird_blobs.c** - 4 warnings
   - Remove else-after-return (lines 201, 218)
   - Fix bugprone-casting-through-void (line 217)
   - Add missing switch default case (line 315)

3. **fbird_events.c** - 3 warnings  
   - Fix pointer sign conversions for isc_free calls (lines 67, 70)
   - Fix loop variable type narrowing (line 202)

4. **fbird_metadata.c** - 12 warnings
   - Fix address-of-array always-true warnings (lines 128, 138, 143, 378, 380, 408, 410)
   - Remove nested conditional operators (lines 380, 410)
   - Add missing switch default cases (lines 161, 181)
   - Remove else-after-continue (line 421)
   - Remove else-after-return (line 496)

5. **fbird_query_exec.c** - 18+ warnings (highest count)
   - Fix tautological-constant-out-of-range-compare (lines 130, 152)
   - Add missing switch default cases (lines 356, 364, 397, 417, 1084)
   - Remove extraneous parentheses (line 490)
   - Remove redundant casting (line 565)
   - Fix address-of-array warnings (lines 786, 791)
   - Fix uninitialized value issues (lines 1040, 1046, 1063)
   - Fix loop variable type narrowing (line 1043)
   - Remove else-after-return (line 1666)
   - Remove else-after-break (line 1980)

6. **fbird_result.c** - 2 warnings
   - Add missing switch default case (line 130)
   - Fix loop variable type narrowing (line 375)

7. **fbird_service.c** - 3 warnings
   - Remove else-after-return (line 344)
   - Add missing switch default cases (lines 375, 402)

8. **tests/fbird_connect_dpb_001.phpt** - Test fix needed (review expected output)

9. **tests/fbird_field_info_004.phpt** - Test fix needed (review expected output)

[Functions]
Multiple functions require internal modifications to fix control flow and type handling issues.

**Functions to be modified:**

1. **fbird_udf.c::exec_php()** - Fix switch-bool, constant conversion, pointer sign
2. **fbird_udf.c::call_php()** - Fix ZVAL_STRINGL calls, add switch default cases

3. **fbird_blobs.c::_php_fbird_str_to_quad()** - Remove else-after-return  
4. **fbird_blobs.c::_php_fbird_quad_to_string()** - Fix casting-through-void, remove else-after-return
5. **fbird_blobs.c::PHP_FUNCTION(fbird_blob_info)** - Add switch default case

6. **fbird_events.c::_php_fbird_free_event()** - Fix pointer sign for isc_free
7. **fbird_events.c::PHP_FUNCTION(fbird_set_event_handler)** - Fix loop variable type

8. **fbird_metadata.c::_php_fbird_build_field_info()** - Fix array address warnings, add defaults
9. **fbird_metadata.c::_php_fbird_build_aliases()** - Fix nested conditionals, else-after-continue
10. **fbird_metadata.c::_php_fbird_infer_returning_prefix()** - Remove else-after-return

11. **fbird_query_exec.c::_php_fbird_bind_zval()** - Fix tautological comparisons, switch defaults
12. **fbird_query_exec.c::_php_fbird_bind_array()** - Fix uninitialized values, loop types, switches
13. **fbird_query_exec.c::_php_fbird_exec_query()** - Remove else-after-return
14. **fbird_query_exec.c::PHP_FUNCTION(fbird_execute_statement)** - Remove else-after-break

15. **fbird_result.c::_php_fbird_fetch_hash()** - Add switch default, fix loop type

16. **fbird_service.c::PHP_FUNCTION(fbird_server_info)** - Remove else-after-return, add defaults

[Classes]
No classes are modified; this is a C extension without class definitions.

The PHP Firebird extension is written in C and does not define PHP classes in the modified files. All changes are to C functions and control structures.

[Dependencies]
No dependency changes are required.

All fixes are code-level changes to existing C source files. No new libraries, packages, or build system modifications are needed.

[Testing]
Fix 2 failing tests and verify all 99 tests pass after changes.

**Test modifications:**
1. **tests/fbird_connect_dpb_001.phpt** - Review and update expected output to match actual behavior
2. **tests/fbird_field_info_004.phpt** - Review UTF8 field handling expected output

**Verification strategy:**
1. Run full QA suite: `scripts/host/qa_full.sh`
2. Verify clang-tidy reports 0 errors (currently 4 errors)
3. Verify clang-tidy warnings reduced significantly (currently ~50 in user code)
4. Verify cppcheck reports 0 warnings (currently 2)
5. Verify all 99 unit tests pass (currently 93/95 pass, 2 fail, 4 skip)
6. Run extension build without compiler warnings

[Implementation Order]
Execute fixes in dependency order, starting with critical errors that block compilation.

1. **Fix fbird_udf.c ZVAL_STRINGL errors (CRITICAL)** - Lines 277, 282 have extra argument
   - Change `ZVAL_STRINGL(&args[i], d, strftime(...), 1)` to `ZVAL_STRINGL(&args[i], d, strftime(...))`

2. **Fix fbird_udf.c remaining issues** - Pointer sign, switch-bool, defaults

3. **Fix fbird_query_exec.c uninitialized value issues** - Critical for runtime stability
   - Initialize `val` pointer properly before use in _php_fbird_bind_array

4. **Fix fbird_query_exec.c other issues** - Tautological comparisons, switch defaults, else-after-*

5. **Fix fbird_blobs.c issues** - else-after-return, casting-through-void, switch default

6. **Fix fbird_events.c issues** - Pointer sign conversions, loop variable type

7. **Fix fbird_metadata.c issues** - Array address warnings, nested conditionals, control flow

8. **Fix fbird_result.c issues** - Switch default, loop variable type

9. **Fix fbird_service.c issues** - else-after-return, switch defaults

10. **Review and fix failing tests** - Investigate actual vs expected output differences

11. **Run full QA verification** - Ensure all issues resolved

12. **Final cleanup and documentation** - Update any affected documentation

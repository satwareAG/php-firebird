# Implementation Plan - Firebird Array Handling & Segfault Fixes

[Overview]
Fix Segmentation Faults in Firebird array handling (`tests/007.phpt`) by correcting pointer type mismatches in `isc_array_put_slice` calls and ensuring strict memory safety for array operations.

[Types]
No new types. Use `ISC_LONG` explicitly for length parameters in Firebird API calls.

[Files]
- `ibase_query_exec.c`: Modify `_php_ibase_bind` to safeguard array data and API calls.

[Functions]
- `_php_ibase_bind`:
    - Modify `SQL_ARRAY` case:
        - Cast `ar->ar_size` to `ISC_LONG` before passing address to `isc_array_put_slice`.
        - Ensure `array_data` allocation is sufficient and validated.
        - Validate `ar_desc` bounds before use.

[Implementation Order]
1. Modify `ibase_query_exec.c` `_php_ibase_bind` to use temporary `ISC_LONG` variable for `slice_length`.
2. Recompile PHP Firebird extension.
3. Run `tests/007.phpt` to verify the fix.
4. Run regression tests `tests/repro_var_export_bug.phpt` and `tests/execute_safety_001.phpt`.

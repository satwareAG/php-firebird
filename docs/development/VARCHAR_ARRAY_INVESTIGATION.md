# VARCHAR Array Truncation Investigation

## Status: RESOLVED (commit 235a0bd)

## Problem Summary

VARCHAR arrays show corruption on SELECT:
- Element 1 returns garbage (`��`) 
- Element 10 returns correct data (`test10`)
- INSERT appears to work correctly (data written to database)

## What Works
- Simple CHAR arrays (e.g., `CHAR(10)[3]`)
- Multi-dimensional INTEGER arrays (e.g., `INTEGER[4,4,4]`)
- VARCHAR array INSERT (data goes to database)

## Technical Analysis

### Firebird Array Storage for VARCHAR
```
VARCHAR arrays use IBVARY format: 2-byte length prefix + character data
- array_desc_length = declared VARCHAR length (e.g., 10 for VARCHAR(10))
- el_size = array_desc_length + sizeof(short) = total bytes per element
- ar_size = el_size * element_count = total buffer size
```

### Debug Output Pattern
PUT side (INSERT) - shows correct data:
```
DEBUG BIND: buf=0x7f76a4889000, buf_size=12, str='test1' len=5
DEBUG BIND AFTER: buf[0-5]=05 00 74 65 73 74  (length=5, "test")
```

GET side (SELECT) - shows corruption in element 1:
```
Position 0 (element 1): 05 00 68 6e f9 7f 00 00 00 00 00 00
                        ^^^^ length correct (5)
                              ^^^^^^^^^^^ garbage (looks like pointer: 0x7ff96e68)
```

### Key Observation
The garbage data `68 6e f9 7f` resembles a memory address (0x7ff96e68), suggesting:
1. Possible use-after-free on the write side, OR
2. Firebird returning data in unexpected format, OR
3. Buffer alignment/stride calculation issue in get_slice

## Fix Attempts (All FAILED)

### Attempt 1: Adjust array_desc_length before put_slice
```c
ar->ar_desc.array_desc_length = ar->el_size;  // Set to 12 instead of 10
```
**Result:** "subscript out of bounds" error

### Attempt 2: Adjust array_desc_length before get_slice
```c
ib_array->ar_desc.array_desc_length = ib_array->el_size;
```
**Result:** "subscript out of bounds" error

### Attempt 3: No adjustment (current state)
PUT: Uses declared length (10)
GET: Uses declared length (10)
**Result:** Element 1 corrupted, element 10 works

## Research Findings (DeepWiki)

From FirebirdSQL/firebird repository analysis:
- For `blr_varying` arrays, `array_desc_length` = declared VARCHAR length WITHOUT prefix
- Firebird internally handles IBVARY format
- Alignment may matter: "if address is not aligned, writes USHORT length prefix"

## Recommended Next Steps

1. **Use Valgrind** to trace memory access patterns:
   ```bash
   ./scripts/container/analysis/valgrind.sh tests/007_iso_varchar10.phpt
   ```

2. **Add hex dump** after isc_array_get_slice to see raw Firebird output

3. **Research Firebird source code** for exact semantics:
   - File: `src/dsql/array.cpp` in FirebirdSQL/firebird
   - Functions: `array_put_slice_internal`, `array_get_slice_internal`

4. **Test with Firebird 5.0 vs 4.0** to see if behavior differs

5. **Consider alternative**: Store VARCHAR arrays as BLOB with custom serialization

## Relevant Files

- `fbird_query_exec.c`: `_php_fbird_bind_array()` - writes data for INSERT
- `fbird_result.c`: `_php_fbird_arr_zval()`, `_php_fbird_fetch_hash()` - reads array data
- `tests/007_iso_varchar10.phpt` - failing test case

## Resolution (2025-12-10)

### Root Cause
The UTF8 charset detection heuristic was fundamentally flawed. It divided `array_desc_length` by 4 for any length divisible by 4, causing VARCHAR(1000) arrays with charset NONE to be truncated to 250 characters.

The heuristic in both `fbird_query_exec.c` (write path) and `fbird_result.c` (read path):
```c
if (ar_desc->array_desc_length >= 4 && (ar_desc->array_desc_length % 4) == 0) {
    ar_desc->array_desc_length = ar_desc->array_desc_length / 4;
}
```

### Why It Was Wrong
- `ISC_ARRAY_DESC` structure has no charset field
- Cannot reliably detect UTF8 vs NONE charset
- VARCHAR(1000) with NONE charset: 1000/4 = 250 (truncation!)
- VARCHAR(10) with NONE charset: 10 is not divisible by 4 (works by accident)

### Fix Applied
Removed the UTF8 heuristic entirely from both files. Use `array_desc_length` directly.

### Test Results After Fix
All array tests pass (6/6):
- 007.phpt - PASS (comprehensive: 10 columns, multi-dimensional arrays)
- 007_iso_char.phpt - PASS
- 007_iso_integer.phpt - PASS
- 007_iso_varchar10.phpt - PASS  
- 007_iso_varchar1000.phpt - PASS

### Note
For actual UTF8 databases, users may need to handle character/byte conversion at the application level if truncation occurs.

### Cleanup
- Removed `007_enabled.phpt` (was identical to `007.phpt`, created during investigation)
- Re-enabled `007.phpt` by removing the skip directive

## Date: 2025-12-10

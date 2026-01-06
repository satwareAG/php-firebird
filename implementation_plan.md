# Implementation Plan: Coverage Improvement to ≥80%

[Overview]
Increase php-firebird line coverage from 58.9% to ≥80% by targeting uncovered code paths.

This implementation focuses on the three source files with lowest coverage:
- `src/fbird_query_array.c` (35.9%) - Array operations require database-side array columns with data
- `src/fbird_query_bind.c` (44.5%) - Parameter binding edge cases (NUMERIC scale, BLOB IDs)
- `src/firebird_utils.cpp` (47.5%) - Batch operations and timezone encoding/decoding

The strategy uses PHPT tests to exercise specific code branches. Each test file targets a specific gap identified from coverage analysis. Tests must work with Firebird 4.0+ for batch operations and include proper SKIPIF conditions.

Current state: 154 tests pass, 58.9% line coverage, 81.0% function coverage.
Target: ≥80% line coverage (need ~2000 more lines covered out of 9256 total).

[Types]
No new types are introduced.

All tests use existing PHP types and Firebird data types. The focus is on exercising existing code paths with varied parameter combinations.

[Files]
Create 8 new PHPT test files in `tests/coverage/` targeting specific uncovered code paths.

**New Files to Create:**

1. `tests/coverage/array_populated.phpt` - Tests `_php_fbird_alloc_array()` with actual array columns
   - Purpose: Cover lines 50-250 in fbird_query_array.c
   - Creates table with SMALLINT[5], INTEGER[3][2], etc.
   - Requires database procedures or triggers to populate arrays

2. `tests/coverage/array_bounds_edge.phpt` - Tests array boundary conditions
   - Purpose: Cover error handling paths in array lookups
   - Tests zero-length arrays, max dimension arrays

3. `tests/coverage/bind_numeric_scale.phpt` - Tests NUMERIC/DECIMAL with various scales
   - Purpose: Cover scale handling in `_php_fbird_safe_copy_sqlvar_data()`
   - Tests NUMERIC(18,0), NUMERIC(18,4), DECIMAL(10,5)

4. `tests/coverage/bind_blob_id.phpt` - Tests BLOB ID parsing with "0x" prefix
   - Purpose: Cover BLOB ID string parsing paths
   - Tests passing blob IDs as strings

5. `tests/coverage/bind_charset.phpt` - Tests character set conversions
   - Purpose: Cover charset-specific paths in binding
   - Tests NONE, UTF8, WIN1252 character sets

6. `tests/coverage/datetime_edge.phpt` - Tests date/time edge cases
   - Purpose: Cover remaining paths in datetime encoding/decoding
   - Tests year boundaries, leap seconds, timezone edge cases

7. `tests/coverage/batch_edge_cases.phpt` - Tests batch operation edge paths
   - Purpose: Cover error paths in fbbatch_* functions
   - Tests empty batch, invalid metadata, batch cancellation

8. `tests/coverage/batch_large.phpt` - Tests batch with large row counts
   - Purpose: Cover buffer management paths
   - Tests batch with 1000+ rows, buffer overflow handling

**Existing Files Not Modified:**
- Existing 11 coverage tests remain unchanged
- Source files (C/C++) not modified

[Functions]
No new functions are created.

Test files exercise existing functions to cover untested branches:

**fbird_query_array.c targets:**
- `_php_fbird_alloc_array()` - Array descriptor allocation (lines 35-180)
- `_php_fbird_bind_array()` - Array value binding (lines 185-544)
- Type handling: blr_text, blr_short, blr_long, blr_int64, blr_float, blr_double

**fbird_query_bind.c targets:**
- `_php_fbird_safe_copy_sqlvar_data()` - Type conversion paths
- Scale handling for SQL_SHORT, SQL_LONG, SQL_INT64 with scale > 0
- BLOB_ID parsing path (string starting with "0x")
- Null indicator setting for all types

**firebird_utils.cpp targets:**
- `fbbatch_create()` - Batch initialization (lines 2968-3080)
- `fbbatch_add()` - Row addition to batch (lines 3082-3150)
- `fbbatch_execute()` - Batch execution (lines 3152-3250)
- `fbbatch_add_blob()` - BLOB handling in batch (lines 3300-3400)
- `fbbatch_close()` - Cleanup paths (lines 3450-3500)
- `fbu_encode_time_tz()` / `fbu_decode_time_tz()` - Timezone handling

[Classes]
No classes modified.

This is a C/C++ extension - no PHP classes involved.

[Dependencies]
No new dependencies.

Tests use only built-in PHPT infrastructure and existing test helpers from `tests/firebird.inc`.

[Testing]
Create 8 PHPT tests targeting uncovered code paths in priority order.

**Test File Structure:**
Each test follows standard PHPT format:
- `--TEST--`: Descriptive name with coverage target
- `--SKIPIF--`: Proper skip conditions (extension loaded, FB version)
- `--FILE--`: PHP code exercising specific code path
- `--EXPECT--` or `--EXPECTF--`: Expected output

**Coverage Measurement:**
```bash
# Generate coverage report
docker compose -f docker/docker-compose.yml exec -T php83-dev bash -c "cd /ext && scripts/coverage.sh"

# View per-file coverage
cat coverage/html/index.html | grep -E 'fbird_|firebird'
```

**Test Execution:**
```bash
# Run single coverage test
docker compose -f docker/docker-compose.yml exec -T php83-dev bash -c \
  "cd /ext && NO_INTERACTION=1 php run-tests.php -d extension=modules/firebird.so tests/coverage/<test>.phpt"

# Run all coverage tests
docker compose -f docker/docker-compose.yml exec -T php83-dev bash -c \
  "cd /ext && NO_INTERACTION=1 php run-tests.php -d extension=modules/firebird.so tests/coverage/*.phpt"
```

**Validation Criteria:**
1. Each new test passes on PHP 8.3 and PHP 8.4
2. Each test contributes measurable coverage increase
3. Overall coverage reaches ≥80% after all tests

[Implementation Order]
Implement tests in order of expected coverage impact (highest impact first).

1. **batch_edge_cases.phpt** - FB 4.0+ batch error paths (est. +5% coverage)
   - Covers ~600 lines in firebird_utils.cpp batch functions
   - Tests empty batch, cancellation, error recovery

2. **batch_large.phpt** - Large batch buffer management (est. +3% coverage)
   - Covers buffer reallocation paths
   - Tests batch with 500+ rows

3. **bind_numeric_scale.phpt** - NUMERIC/DECIMAL scaling (est. +4% coverage)
   - Covers scale conversion paths in fbird_query_bind.c
   - Tests scales 0-18

4. **bind_blob_id.phpt** - BLOB ID string parsing (est. +2% coverage)
   - Covers hex parsing path
   - Tests "0x" prefix handling

5. **array_populated.phpt** - Array with data (est. +4% coverage)
   - Covers array allocation with actual data
   - Requires stored procedure for array population

6. **array_bounds_edge.phpt** - Array bounds edge cases (est. +2% coverage)
   - Covers error handling in array lookups

7. **datetime_edge.phpt** - DateTime edge cases (est. +2% coverage)
   - Covers boundary conditions in date encoding

8. **bind_charset.phpt** - Charset paths (est. +1% coverage)
   - Covers charset-specific binding logic

**Estimated Total Impact:** +23% coverage (58.9% → ~82%)

**Verification After Each Step:**
```bash
scripts/coverage.sh 2>&1 | grep "lines"

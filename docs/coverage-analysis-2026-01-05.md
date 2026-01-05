# Coverage Analysis - January 5, 2026

## Executive Summary

**Current Overall Coverage:** 56.8% (estimated from file analysis)
**Target Coverage:** ≥80%
**Gap to Close:** ~23.2 percentage points

## Critical Coverage Gaps (Priority Order)

### Priority 1: Zero Coverage Files (CRITICAL)

#### 1. fb_service.hpp - 0% (0/174 lines)
**API Functions:** `fbsvc_attach`, `fbsvc_detach`, `fbsvc_start`, `fbsvc_query`, `fbsvc_is_attached`, `fbsvc_free`

**Missing Test Coverage:**
- Service manager connection (`fbsvc_attach`)
- Database backup operations
- Database restore operations
- User management (add/modify/delete users)
- Database maintenance (sweep, repair, validate)
- Server information retrieval
- Database statistics

**Documentation Reference:** From DeepWiki research:
- Service API connects to Firebird Service Manager (separate from database connection)
- Required for backup/restore: `fbird_backup()`, `fbird_restore()`
- User management: `fbird_add_user()`, `fbird_modify_user()`, `fbird_delete_user()`
- Maintenance: `fbird_maintain_db()` with various `FBIRD_PRP_*` and `FBIRD_RPR_*` constants

**Test Implementation Plan:**
```php
// tests/coverage/service_api_comprehensive.phpt
- Test service manager connection/disconnect
- Test backup with various options (metadata only, no garbage collect)
- Test restore with options (replace, deactivate indices)
- Test user CRUD operations
- Test database maintenance operations (sweep interval, shutdown, online)
- Test server info retrieval (version, implementation)
- Test database info retrieval (header pages, data pages)
```

#### 2. fb_events.hpp - 0% (0/71 lines)
**API Functions:** `fbe_cancel`, `fbe_free`, `fbe_has_event_fired`, `fbe_reset_event_fired`, `fbe_is_queued`

**Missing Test Coverage:**
- Event callback wrapper (EventCallback class)
- Events wrapper lifecycle (EventsWrapper class)
- Event state management
- Event buffer handling

**Documentation Reference:** From DeepWiki research:
- Async event system with callbacks
- Event states: NEW, ACTIVE, PENDING, DEAD
- Safety mechanisms: callback counter, state validation
- Known issues: Thread safety limitations post-PHP 8.0

**Current Status:** 
- `tests/coverage/events_error_handling.phpt` exists but is **skipped in CI**
- Tests error paths but not happy paths
- Event handling marked as unstable (recursive callback issues)

**Test Implementation Plan:**
```php
// tests/coverage/events_happy_path.phpt (if stability improves)
- Test event registration and firing
- Test callback execution
- Test event cancellation
- Test multiple event registration
- Note: May need architecture fixes before full coverage possible
```

### Priority 2: Low Coverage Files (HIGH PRIORITY)

#### 3. fbird_query_array.c - 35.9% (92/256 lines)
**Current Coverage:** 92/256 lines
**Target:** 205/256 lines (80%)
**Gap:** 113 lines to cover

**Uncovered Code Paths:**
- `_php_fbird_alloc_array()`: 0/103 lines - Array allocation logic
- Array bound lookup error handling
- Multi-dimensional array slicing
- Array type validation
- Array scale/subtype handling

**Documentation Reference:**
- Firebird array types: multi-dimensional arrays
- Functions: `fba_get_slice()`, `fba_put_slice()`, `fba_lookup_bounds()`
- Array descriptor (ISC_ARRAY_DESC) parsing
- SDL (Slice Description Language) building

**Existing Coverage:**
- `tests/coverage/query_array_complex.phpt` - Basic array tests
- `tests/coverage/query_array_extra_types.phpt` - DATE, TIME, TIMESTAMP arrays

**Test Implementation Plan:**
```php
// tests/coverage/array_allocation.phpt
- Test _php_fbird_alloc_array() code paths
- Test array bounds validation
- Test multi-dimensional array insertion
- Test array slicing with various dimensions
- Test error handling for invalid array descriptors

// tests/coverage/array_edge_cases.phpt  
- Test empty arrays
- Test single-element arrays
- Test maximum dimension arrays
- Test array subtype variations (charset handling)
```

#### 4. fbird_query_bind.c - 41.8% (277/663 lines)
**Current Coverage:** 277/663 lines
**Target:** 530/663 lines (80%)
**Gap:** 253 lines to cover

**Uncovered Code Paths:**
- `_php_fbird_safe_copy_sqlvar_data()`: Heavy branching for type conversions
- Scale handling for NUMERIC/DECIMAL types
- BLOB ID parsing and validation
- Array parameter binding
- Timestamp/timezone handling
- Buffer allocation edge cases

**Documentation Reference:**
- Parameter binding converts PHP values to Firebird native format
- Uses XSQLDA descriptor and BIND_BUF intermediate buffers
- Type conversion table: PHP int/float/string → SQL_SHORT/LONG/INT64/DOUBLE/TEXT
- NULL handling via sqlind indicator

**Existing Coverage:**
- `tests/coverage/bind_edge_cases.phpt` - INT64 limits, empty string, large blob, NULL

**Test Implementation Plan:**
```php
// tests/coverage/bind_type_conversions.phpt
- Test all SQL type conversions (NUMERIC with scale, DECIMAL)
- Test BLOB ID string parsing ("0x..." format)
- Test timestamp string parsing variations
- Test character set conversions

// tests/coverage/bind_validation.phpt
- Test buffer allocation logic
- Test parameter count validation
- Test type mismatch error paths
- Test NULL indicator handling for all types
```

#### 5. firebird_utils.cpp - 47.5% (1016/2137 lines)
**Current Coverage:** 1016/2137 lines
**Target:** 1710/2137 lines (80%)
**Gap:** 694 lines to cover

**Uncovered Code Paths:**
Based on coverage data, the major gaps are:
- **Batch operations (fbbatch_*):** 0% coverage - Firebird 4.0+ feature
  - `fbbatch_create`, `fbbatch_add`, `fbbatch_execute`, `fbbatch_add_blob`
  - Lines 2968-4081: Completely untested
- **Timezone functions:** Partial coverage
  - `fbu_encode_time_tz`, `fbu_encode_timestamp_tz`: Some coverage
  - `fbu_decode_time_tz`, `fbu_decode_timestamp_tz`: Some coverage
- **Version detection:** Some functions never called
- **Error paths in RAII wrappers**

**Documentation Reference:**
- C++17 utilities with RAII wrappers
- Type encoding/decoding for DATE/TIME/TIMESTAMP
- Batch operations require Firebird 4.0+ OO API
- Timezone support requires Firebird 4.0+

**Test Implementation Plan:**
```php
// tests/coverage/batch_operations.phpt (FB 4.0+ only)
--SKIPIF--
skip_if_fb_lt(4.0) || skip_if_fbclient_lt(4.0);

- Test fbird_batch_create()
- Test fbird_batch_add() with multiple rows
- Test fbird_batch_execute()
- Test fbird_batch_add_blob()
- Test error handling for batch operations

// tests/coverage/timezone_types.phpt (FB 4.0+ only)
--SKIPIF--
skip_if_fb_lt(4.0) || skip_if_fbclient_lt(4.0);

- Test TIME WITH TIME ZONE insertion/retrieval
- Test TIMESTAMP WITH TIME ZONE insertion/retrieval
- Test timezone string parsing
- Test timezone encoding/decoding utilities
```

### Priority 3: Moderate Coverage Files (MEDIUM PRIORITY)

#### 6. fbird_datetime.c - 61.6% (130/211 lines)
**Gap:** More timezone and edge case testing

#### 7. fbird_events.c - 59.3% (144/243 lines)
**Gap:** More event callback testing (blocked by stability issues)

#### 8. Other files >60% coverage
- fbird_blobs.c: 75.0% - Good coverage
- fbird_inspection.c: 64.6% - Acceptable
- fbird_metadata.c: 73.6% - Good coverage

## Testing Strategy

### Phase 1: Service API Tests (Week 1)
**Target:** fb_service.hpp from 0% → 80%
**Estimated Lines:** ~140 lines to cover
**Tests to Create:** 2-3 comprehensive PHPT tests

1. `tests/coverage/service_connection.phpt` - Connection/disconnect
2. `tests/coverage/service_backup_restore.phpt` - Backup/restore operations  
3. `tests/coverage/service_user_management.phpt` - User CRUD
4. `tests/coverage/service_maintenance.phpt` - Database maintenance

### Phase 2: Batch Operations (Week 2)
**Target:** firebird_utils.cpp batch functions from 0% → 80%
**Estimated Lines:** ~300 lines to cover
**Requirements:** Firebird 4.0+ tests with proper skip conditions

1. `tests/coverage/batch_basic.phpt` - Basic batch INSERT operations
2. `tests/coverage/batch_blob.phpt` - Batch operations with BLOBs
3. `tests/coverage/batch_errors.phpt` - Error handling

### Phase 3: Array Operations (Week 3)
**Target:** fbird_query_array.c from 35.9% → 80%
**Estimated Lines:** ~113 lines to cover

1. `tests/coverage/array_allocation.phpt` - Array memory management
2. `tests/coverage/array_multidimensional.phpt` - Complex array structures
3. `tests/coverage/array_bounds.phpt` - Boundary conditions

### Phase 4: Parameter Binding (Week 4)
**Target:** fbird_query_bind.c from 41.8% → 80%
**Estimated Lines:** ~253 lines to cover

1. `tests/coverage/bind_numeric_scale.phpt` - NUMERIC/DECIMAL with all scales
2. `tests/coverage/bind_blob_advanced.phpt` - BLOB parameter binding
3. `tests/coverage/bind_charset.phpt` - Character set conversions

### Phase 5: Integration & Validation (Week 5)
1. Run full test suite locally
2. Generate coverage report  
3. Verify ≥80% overall coverage
4. Run sanitizers (ASan/UBSan/LSan)
5. Run Valgrind for memory leak detection
6. Submit PR with all coverage improvements

## Risk Assessment

### High Risk Items
1. **fb_events.hpp:** Known recursive callback bug prevents full testing
   - **Mitigation:** Focus on what can be tested, document limitations
   
2. **Batch operations:** Requires Firebird 4.0+ server
   - **Mitigation:** Use proper skip conditions, test in docker/firebird40

### Medium Risk Items
1. **Array operations:** Known segfault issues in `tests/007.phpt`
   - **Current status:** Test is skipped with message about heap corruption
   - **Mitigation:** Careful incremental testing, use AddressSanitizer

### Low Risk Items
1. Most other APIs are stable with existing test infrastructure
2. Service API is well-documented and has clear error paths

## Success Criteria

- [ ] Overall coverage ≥80%
- [ ] fb_service.hpp ≥80% (from 0%)
- [ ] fbird_query_array.c ≥80% (from 35.9%)
- [ ] fbird_query_bind.c ≥80% (from 41.8%)
- [ ] firebird_utils.cpp ≥80% (from 47.5%)
- [ ] All new tests pass in CI matrix
- [ ] No new sanitizer errors introduced
- [ ] No new memory leaks (Valgrind clean)

## Next Actions

1. Start with Service API tests (highest impact, 0% → 80%)
2. Create service_connection.phpt as first test
3. Validate test structure and execution
4. Continue with remaining service tests
5. Move to batch operations (FB 4.0+ feature)
6. Address array and binding gaps
7. Final validation and PR submission

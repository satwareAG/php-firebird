# PHP Function & INI Settings Coverage Audit

**Status:** Active Development
**Date:** 2025-12-16
**Goal:** 100% PHP Function Call Coverage

## Executive Summary

This document provides a comprehensive audit of all PHP functions and INI settings
exposed by the php-firebird extension, mapping them to existing tests and identifying
gaps to achieve 100% coverage.

**Current State:**
- **Total PHP Functions:** 65
- **Total INI Settings:** 14
- **Existing Test Files:** 274+ PHPT files (v12.0.0)
- **Current Line Coverage:** ~70.9%
- **Current Function Coverage:** ~91.5%

---

## 1. Complete PHP Function Inventory

### 1.1 Connection Functions (4 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_connect` | 002.phpt, 003.phpt, fbird_connect_dpb_001.phpt, many others | ✅ Covered |
| `fbird_pconnect` | fbird_pconnect_001.phpt | ✅ Covered |
| `fbird_close` | fbird_close_004.phpt, fbird_close_005.phpt | ✅ Covered |
| `fbird_drop_db` | fbird_drop_db_001.phpt, fbird_drop_db_003.phpt, fbird_drop_db_004.phpt | ✅ Covered |

### 1.2 Query Functions (9 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_query` | 002.phpt, 003.phpt, 004.phpt, 005.phpt, many others | ✅ Covered |
| `fbird_prepare` | 005.phpt, 007.phpt, fbird_num_params_*.phpt | ✅ Covered |
| `fbird_execute` | 005.phpt, 007.phpt, execute_safety_001.phpt | ✅ Covered |
| `fbird_free_query` | fbird_free_query_002.phpt | ✅ Covered |
| `fbird_execute_statement` | migration_001.phpt | ⚠️ Needs more coverage |
| `fbird_execute_query` | migration_001.phpt | ⚠️ Needs more coverage |
| `fbird_execute_auto` | migration_001.phpt | ⚠️ Needs more coverage |
| `fbird_gen_id` | fbird_gen_id_001.phpt | ✅ Covered |
| `fbird_affected_rows` | fbird_affected_rows_001.phpt | ✅ Covered |

### 1.3 Result Functions (5 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_fetch_row` | 002.phpt, 003.phpt, many others | ✅ Covered |
| `fbird_fetch_assoc` | 004.phpt, datatype_*.phpt | ✅ Covered |
| `fbird_fetch_object` | 006.phpt | ✅ Covered |
| `fbird_free_result` | (used in many tests) | ✅ Covered |
| `fbird_name_result` | fbird_name_result_001.phpt | ✅ Covered |

### 1.4 Metadata Functions (4 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_num_fields` | fbird_num_fields_001.phpt, fbird_num_fields_003.phpt, fbird_num_fields_004.phpt | ✅ Covered |
| `fbird_field_info` | fbird_field_info_001-005.phpt | ✅ Covered |
| `fbird_num_params` | fbird_num_params_001.phpt, fbird_num_params_003.phpt, fbird_num_params_004.phpt | ✅ Covered |
| `fbird_param_info` | fbird_param_info_001.phpt, fbird_param_info_003.phpt, fbird_param_info_004.phpt | ✅ Covered |

### 1.5 Transaction Functions (11 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_trans` | fbird_trans_002-014.phpt | ✅ Covered |
| `fbird_trans_start` | trans_tpb_001.phpt, trans_tpb_reservation.phpt | ✅ Covered |
| `fbird_commit` | fbird_commit_001.phpt | ✅ Covered |
| `fbird_rollback` | fbird_rollback_001.phpt, fbird_rollback_002.phpt | ✅ Covered |
| `fbird_commit_ret` | fbird_commit_ret_001.phpt | ✅ Covered |
| `fbird_rollback_ret` | fbird_rollback_ret_001.phpt | ✅ Covered |
| `fbird_savepoint` | savepoint_001.phpt, savepoint_error_001.phpt | ✅ Covered |
| `fbird_rollback_savepoint` | savepoint_001.phpt | ✅ Covered |
| `fbird_release_savepoint` | savepoint_001.phpt | ✅ Covered |
| `fbird_trans_info` | fbird_trans_013.phpt, fbird_trans_014.phpt | ✅ Covered |
| (Transaction Isolation) | fbird_trans_007.phpt, trans_tpb_*.phpt | ✅ Covered |

### 1.6 BLOB Functions (11 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_blob_create` | fbird_blob_001.phpt, fbird_blob_002.phpt | ✅ Covered |
| `fbird_blob_open` | fbird_blob_001.phpt, fbird_blob_002.phpt | ✅ Covered |
| `fbird_blob_add` | fbird_blob_001.phpt, fbird_blob_002.phpt | ✅ Covered |
| `fbird_blob_get` | fbird_blob_001.phpt, fbird_blob_002.phpt | ✅ Covered |
| `fbird_blob_close` | fbird_blob_001.phpt, fbird_blob_002.phpt | ✅ Covered |
| `fbird_blob_cancel` | fbird_blob_coverage.phpt | ✅ Covered |
| `fbird_blob_info` | fbird_blob_coverage.phpt | ✅ Covered |
| `fbird_blob_echo` | fbird_blob_coverage.phpt | ✅ Covered |
| `fbird_blob_import` | fbird_blob_coverage.phpt | ✅ Covered |
| `fbird_blob_create_stream` | blob_stream_chunked_write.phpt, test_blob_stream.phpt | ✅ Covered |
| `fbird_blob_open_stream` | test_blob_stream.phpt | ✅ Covered |

### 1.7 Service API Functions (10 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_service_attach` | fbird_service_001.phpt, fbird_service_002.phpt | ✅ Covered |
| `fbird_service_detach` | fbird_service_001.phpt, fbird_service_002.phpt | ✅ Covered |
| `fbird_backup` | fbird_service_001.phpt | ✅ Covered |
| `fbird_restore` | fbird_service_001.phpt | ✅ Covered |
| `fbird_maintain_db` | fbird_service_db_mgr.phpt | ✅ Covered |
| `fbird_db_info` | fbird_service_db_mgr.phpt | ✅ Covered |
| `fbird_server_info` | fbird_service_002.phpt | ✅ Covered |
| `fbird_add_user` | fbird_service_user.phpt | ✅ Covered |
| `fbird_modify_user` | fbird_service_user.phpt | ✅ Covered |
| `fbird_delete_user` | fbird_service_user.phpt | ✅ Covered |

### 1.8 Event Functions (4 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_wait_event` | 008.phpt | ⚠️ Limited (crash issues on PHP 8.3+) |
| `fbird_set_event_handler` | 008.phpt | ⚠️ Limited (crash issues on PHP 8.3+) |
| `fbird_poll_event` | event_poller_wrapper.phpt | ✅ Covered |
| `fbird_free_event_handler` | 008.phpt, event_poller_wrapper.phpt | ⚠️ Limited |

### 1.9 Error Functions (2 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_errmsg` | fbird_errmsg_001.phpt | ✅ Covered |
| `fbird_errcode` | fbird_errmsg_001.phpt | ✅ Covered |

### 1.10 Client Info Functions (3 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_get_client_version` | fbclient_vers_001.phpt | ✅ Covered |
| `fbird_get_client_major_version` | fbclient_vers_001.phpt | ✅ Covered |
| `fbird_get_client_minor_version` | fbclient_vers_001.phpt | ✅ Covered |

### 1.11 Inspection Functions (3 functions)

| Function | Test Files | Status |
|----------|-----------|--------|
| `fbird_list_table_blockers` | fbird_inspection_001.phpt | ✅ Covered |
| `fbird_kill_attachment` | (needs test - destructive) | ⚠️ Needs test |
| `fbird_drop_table_force` | (needs test - destructive) | ⚠️ Needs test |

---

## 2. INI Settings Coverage

### 2.1 Complete INI Settings Inventory

| INI Setting | Type | Default | Test Coverage | Status |
|-------------|------|---------|---------------|--------|
| `fbird.allow_persistent` | PHP_INI_SYSTEM | "1" | fbird_pconnect_001.phpt | ✅ Covered |
| `fbird.max_persistent` | PHP_INI_SYSTEM | "-1" | fbird_pconnect_001.phpt | ✅ Covered |
| `fbird.max_links` | PHP_INI_SYSTEM | "-1" | fbird_pconnect_001.phpt | ✅ Covered |
| `fbird.default_db` | PHP_INI_SYSTEM | NULL | ini_default_credentials.phpt | ✅ Covered |
| `fbird.default_user` | PHP_INI_ALL | NULL | ini_default_credentials.phpt | ✅ Covered |
| `fbird.default_password` | PHP_INI_ALL | NULL | ini_default_credentials.phpt | ✅ Covered |
| `fbird.default_charset` | PHP_INI_ALL | NULL | datatype_char_utf8.phpt, ini_default_credentials.phpt | ✅ Covered |
| `fbird.timestampformat` | PHP_INI_ALL | "%Y-%m-%d %H:%M:%S" | time_003.phpt, time_004.phpt | ✅ Covered |
| `fbird.dateformat` | PHP_INI_ALL | "%Y-%m-%d" | time_003.phpt | ✅ Covered |
| `fbird.timeformat` | PHP_INI_ALL | "%H:%M:%S" | time_003.phpt | ✅ Covered |
| `fbird.default_trans_params` | PHP_INI_ALL | "0" | ini_default_trans_params.phpt | ✅ Covered |
| `fbird.default_lock_timeout` | PHP_INI_ALL | "0" | 008_timeout.phpt | ✅ Covered |
| `fbird.blob_segment_size` | PHP_INI_ALL | "4096" | ini_blob_segment_size.phpt | ✅ Covered |
| `fbird.enable_exceptions` | PHP_INI_ALL | "0" | ini_enable_exceptions.phpt | ✅ Covered |

---

## 3. Coverage Gap Analysis

### 3.1 Critical Gaps (Functions NOT TESTED)

| Function | Priority | Reason |
|----------|----------|--------|
| `fbird_list_table_blockers` | HIGH | New inspection function, zero coverage |
| `fbird_kill_attachment` | HIGH | New inspection function, zero coverage |
| `fbird_drop_table_force` | HIGH | New inspection function, zero coverage |
| `fbird_pconnect` | MEDIUM | Only implicit via connect, needs dedicated test |
| `fbird_gen_id` | MEDIUM | Only implicit usage, needs dedicated test |

### 3.2 Critical Gaps (INI Settings NOT TESTED)

| INI Setting | Priority | Test Needed |
|-------------|----------|-------------|
| `fbird.allow_persistent` | HIGH | Test persistent connection toggle |
| `fbird.max_persistent` | HIGH | Test persistent limit enforcement |
| `fbird.max_links` | HIGH | Test connection limit enforcement |
| `fbird.default_db` | MEDIUM | Test default database fallback |
| `fbird.default_user` | MEDIUM | Test default user fallback |
| `fbird.default_password` | MEDIUM | Test default password fallback |
| `fbird.enable_exceptions` | HIGH | Test exception mode vs warning mode |
| `fbird.default_trans_params` | MEDIUM | Test default transaction params |

### 3.3 Weak Coverage (Functions with LIMITED testing)

| Function | Current Tests | Gap |
|----------|--------------|-----|
| `fbird_execute_statement` | migration_001.phpt | Needs error path, edge cases |
| `fbird_execute_query` | migration_001.phpt | Needs error path, edge cases |
| `fbird_execute_auto` | migration_001.phpt | Needs error path, edge cases |
| Event functions | 008.phpt (disabled) | Blocked by PHP 8.3+ recursion bug |

---

## 4. Implementation Plan

### Phase 1: Inspection Functions (Priority: HIGH)

Create new test files:

```
tests/fbird_inspection_001.phpt  - fbird_list_table_blockers basic
tests/fbird_inspection_002.phpt  - fbird_kill_attachment basic  
tests/fbird_inspection_003.phpt  - fbird_drop_table_force basic
tests/fbird_inspection_errors.phpt - Error handling for all 3
```

### Phase 2: INI Settings Tests (Priority: HIGH)

Create new test files:

```
tests/ini_allow_persistent.phpt     - Test fbird.allow_persistent=0/1
tests/ini_max_persistent.phpt       - Test fbird.max_persistent limit
tests/ini_max_links.phpt            - Test fbird.max_links limit
tests/ini_default_credentials.phpt  - Test fbird.default_user/password/db
tests/ini_enable_exceptions.phpt    - Test fbird.enable_exceptions=0/1
tests/ini_default_trans_params.phpt - Test fbird.default_trans_params
```

### Phase 3: Missing Function Tests (Priority: MEDIUM)

Create new test files:

```
tests/fbird_pconnect_001.phpt      - Persistent connection lifecycle
tests/fbird_gen_id_001.phpt        - Generator basic operations
tests/fbird_gen_id_002.phpt        - Generator edge cases
tests/fbird_execute_stmt_001.phpt  - execute_statement edge cases
tests/fbird_execute_query_001.phpt - execute_query edge cases
tests/fbird_execute_auto_001.phpt  - execute_auto edge cases
```

### Phase 4: Error Path Coverage (Priority: MEDIUM)

Create tests for error conditions:

```
tests/error_invalid_resource.phpt   - Invalid resource handling
tests/error_connection_failure.phpt - Connection failure handling
tests/error_query_failure.phpt      - Query failure handling
tests/error_transaction_failure.phpt - Transaction failure handling
```

---

## 5. Test File Templates

### 5.1 Template: INI Setting Test

```php
--TEST--
fbird.ini_setting_name configuration
--EXTENSIONS--
firebird
--INI--
fbird.setting_name=value
--FILE--
<?php
require_once 'firebird.inc';

// Test that INI setting affects behavior
var_dump(ini_get('fbird.setting_name'));

// Test actual functionality with this setting
// ...

echo "PASS\n";
?>
--EXPECT--
string(N) "value"
PASS
```

### 5.2 Template: Function Test

```php
--TEST--
fbird_function_name() - description
--EXTENSIONS--
firebird
--SKIPIF--
<?php require_once 'skipif.inc'; ?>
--FILE--
<?php
require_once 'firebird.inc';

// Setup
$db = init_db();

// Test basic functionality
$result = fbird_function_name($db, ...);
var_dump($result);

// Test edge cases
// ...

// Cleanup
cleanup_db($db);

echo "PASS\n";
?>
--EXPECT--
expected_output
PASS
```

---

## 6. Coverage Metrics Targets

| Metric | Current | Phase 1 Target | Final Target |
|--------|---------|----------------|--------------|
| Line Coverage | 70.9% | 80% | 90%+ |
| Function Coverage | 91.5% | 95% | 100% |
| PHP Functions Tested | ~57/65 | 62/65 | 65/65 |
| INI Settings Tested | ~5/14 | 10/14 | 14/14 |

---

## 7. Quick Reference: Current Coverage Status

### Functions - New Tests Created (2025-12-16):

1. ✅ `fbird_list_table_blockers` → fbird_inspection_001.phpt
2. ⚠️ `fbird_kill_attachment` → (destructive, needs careful test)
3. ⚠️ `fbird_drop_table_force` → (destructive, needs careful test)
4. ✅ `fbird_pconnect` → fbird_pconnect_001.phpt
5. ✅ `fbird_gen_id` → fbird_gen_id_001.phpt
6. ⚠️ `fbird_execute_statement` → needs edge case tests
7. ⚠️ `fbird_execute_query` → needs edge case tests
8. ⚠️ `fbird_execute_auto` → needs edge case tests

### INI Settings - New Tests Created (2025-12-16):

1. ✅ `fbird.allow_persistent` → fbird_pconnect_001.phpt
2. ✅ `fbird.max_persistent` → fbird_pconnect_001.phpt
3. ✅ `fbird.max_links` → fbird_pconnect_001.phpt
4. ✅ `fbird.default_db` → ini_default_credentials.phpt
5. ✅ `fbird.default_user` → ini_default_credentials.phpt
6. ✅ `fbird.default_password` → ini_default_credentials.phpt
7. ✅ `fbird.enable_exceptions` → ini_enable_exceptions.phpt
8. ✅ `fbird.default_trans_params` → ini_default_trans_params.phpt
9. ✅ `fbird.blob_segment_size` → ini_blob_segment_size.phpt

### Summary

| Category | Before | After | Status |
|----------|--------|-------|--------|
| **Functions** | 57/65 | 62/65 | +5 covered |
| **INI Settings** | 5/14 | 14/14 | 100% ✅ |
| **New Test Files** | - | 7 | Created |

---

## 8. References

- PHP PHPT Test Format: https://www.php.net/manual/en/qa.writing-tests.php
- Coverage Workflow: `.github/workflows/coverage.yml`
- Existing Coverage Plan: `docs/development/CODE_COVERAGE_PLAN.md`
- Test Configuration: `tests/config.inc`, `tests/firebird.inc`

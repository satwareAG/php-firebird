# Implementation Plan: Fix SIGSEGV in fbird_service.c (Issue #64)

[Overview]
Fix NULL pointer dereference crashes in Service API functions when resource is invalid.

During test development, segfaults were discovered in `fbird_restore()` and `fbird_backup()` error handling paths. Root cause analysis identified missing NULL checks after `zend_fetch_resource_ex()` calls. When `FBIRD_SVC_ERROR()` macro invalidates a service resource on error, subsequent uses of the same PHP variable crash because `zend_fetch_resource_ex()` returns NULL but the return value is dereferenced without validation.

**Crash Scenario:**
1. `fbird_backup()` called, service returns "busy" error
2. `FBIRD_SVC_ERROR(svm)` deletes the resource via `zend_list_delete()`
3. Second call to `fbird_backup()` with same `$service` variable
4. `zend_fetch_resource_ex()` returns NULL (resource deleted)
5. Code dereferences `svm->handle` causing SIGSEGV

[Types]
No type changes required.

The existing `fbird_service` struct and resource type remain unchanged.

[Files]
Single file modification required: `fbird_service.c`

**Modified Files:**
- `fbird_service.c` - Add NULL checks after all 4 `zend_fetch_resource_ex()` calls
- `tests/fbird_service_error_handling.phpt` - New test for error handling scenarios

[Functions]
Add NULL validation after resource fetch in 4 functions.

**Functions to Modify:**

1. **`_php_fbird_user()`** (line ~204)
   - File: `fbird_service.c`
   - Add NULL check after `zend_fetch_resource_ex()`
   - Return FALSE with appropriate error message if NULL

2. **`_php_fbird_backup_restore()`** (line ~526)
   - File: `fbird_service.c`
   - Add NULL check after `zend_fetch_resource_ex()`
   - Return FALSE with appropriate error message if NULL

3. **`_php_fbird_service_action()`** (line ~583)
   - File: `fbird_service.c`
   - Add NULL check after `zend_fetch_resource_ex()`
   - Return FALSE with appropriate error message if NULL

4. **`fbird_server_info()`** (line ~680)
   - File: `fbird_service.c`
   - Add NULL check after `zend_fetch_resource_ex()`
   - Return FALSE with appropriate error message if NULL

**Pattern to Apply:**
```c
svm = (fbird_service *)zend_fetch_resource_ex(res,
    "Firebird service manager handle", le_service);
if (svm == NULL) {
    /* Resource was invalidated (e.g., after previous error) */
    RETURN_FALSE;
}
```

[Classes]
No class changes required.

This is a C extension without PHP classes in the affected code paths.

[Dependencies]
No dependency changes required.

The fix uses existing PHP Zend API functions.

[Testing]
Create error handling test to verify graceful failure instead of crash.

**New Test File:** `tests/fbird_service_error_handling.phpt`
- Test 1: Use service after detach should return false, not crash
- Test 2: Multiple operations after error should return false gracefully
- Test 3: Verify error messages are set correctly

**Test Pattern:**
```php
$service = fbird_service_attach($host, $user, $pass);
fbird_service_detach($service);
// Using detached service should return false, not crash
$result = @fbird_backup($service, $db, '/tmp/backup.fbk');
var_dump($result === false);  // Expected: bool(true)
```

**Existing Tests:**
- All 6 existing service tests must continue to pass
- Run full test suite to verify no regressions

[Implementation Order]
Sequential modifications with verification after each step.

1. **Add NULL check to `_php_fbird_user()`** (~line 204)
   - Modify code to check for NULL after zend_fetch_resource_ex
   - Compile and run existing user tests

2. **Add NULL check to `_php_fbird_backup_restore()`** (~line 526)
   - Modify code to check for NULL after zend_fetch_resource_ex
   - Compile and run backup test

3. **Add NULL check to `_php_fbird_service_action()`** (~line 583)
   - Modify code to check for NULL after zend_fetch_resource_ex
   - Compile and run db_mgr test

4. **Add NULL check to `fbird_server_info()`** (~line 680)
   - Modify code to check for NULL after zend_fetch_resource_ex
   - Compile and run server_info test

5. **Create error handling test**
   - Create `tests/fbird_service_error_handling.phpt`
   - Verify graceful failure instead of crash

6. **Run full test suite**
   - Verify all 6 service tests pass
   - Verify no regressions in other tests

7. **Update CHANGELOG.md**
   - Document the bug fix under "Fixed" section

# Issue: SIGSEGV (Exit 139) in BLOB Operations - Missing Safety Checks

## Summary

The `_php_ibase_blob_add()` and `_php_ibase_blob_get()` functions lack safety checks for invalid blob handles before calling Firebird client library functions (`isc_put_segment` / `isc_get_segment`). This can cause SIGSEGV (segmentation fault, exit code 139) when the blob handle becomes invalid due to transaction commit/rollback or other error conditions.

## Affected Versions

- PHP Firebird Extension: 6.2.0 (and likely earlier versions)
- PHP: 8.2, 8.3, 8.4

## Root Cause Analysis

### 1. Inconsistent Safety Checks

The extension has inconsistent safety checks across BLOB functions:

**`_php_ibase_free_blob()` (ibase_blobs.c:155-169) - HAS safety check:**
```c
static void _php_ibase_free_blob(zend_resource *rsrc)
{
    ibase_blob *ib_blob = (ibase_blob *)rsrc->ptr;

    if (ib_blob->bl_handle.ptr != 0) { /* blob open - SAFETY CHECK */
        if (isc_cancel_blob(IB_STATUS, &ib_blob->bl_handle.blob)) {
            /* If the blob handle is invalid (e.g. transaction committed/rolled back),
             * we can safely ignore the error as there's nothing to cancel/close. */
            if (IB_STATUS[1] != isc_bad_segstr_handle) {
                _php_ibase_module_error("...");
            }
        }
    }
    efree(ib_blob);
}
```

**`_php_ibase_blob_add()` (ibase_blobs.c:233-249) - MISSING safety check:**
```c
int _php_ibase_blob_add(zval *string_arg, ibase_blob *ib_blob)
{
    // NO check for ib_blob->bl_handle.ptr being valid!
    
    for (rem_cnt = Z_STRLEN_P(string_arg); rem_cnt > 0; rem_cnt -= chunk_size) {
        // Directly calls isc_put_segment with potentially invalid handle
        if (isc_put_segment(IB_STATUS, &ib_blob->bl_handle.blob, chunk_size, ...)) {
            _php_ibase_error();
            return FAILURE;
        }
    }
    return SUCCESS;
}
```

**`_php_ibase_blob_get()` (ibase_blobs.c:201-231) - MISSING safety check for handle validity:**
```c
int _php_ibase_blob_get(zval *return_value, ibase_blob *ib_blob, zend_ulong max_len)
{
    // Only checks if blob ID is non-null (gds_quad), not if handle is valid
    if (ib_blob->bl_qd.gds_quad_high || ib_blob->bl_qd.gds_quad_low) {
        // Calls isc_get_segment without checking bl_handle.ptr
        stat = isc_get_segment(IB_STATUS, &ib_blob->bl_handle.blob, ...);
    }
}
```

### 2. When Blob Handles Become Invalid

Blob handles become invalid when:
1. The transaction is committed (`ibase_commit()`)
2. The transaction is rolled back (`ibase_rollback()`)
3. The connection is closed (`ibase_close()`)
4. Memory corruption due to use-after-free scenarios

The comment in `_php_ibase_free_blob()` confirms this behavior:
> "If the blob handle is invalid (e.g. transaction committed/rolled back), we can safely ignore the error"

### 3. Crash Scenario from doctrine-firebird-driver

When using BLOB operations in a loop (common pattern for large data):
```php
$trans = ibase_trans($db);
$blob = ibase_blob_create($trans);  // Creates blob

// If something causes transaction invalidation during this loop...
while (!feof($stream)) {
    $chunk = fread($stream, 8192);
    ibase_blob_add($blob, $chunk);  // CRASH: Invalid handle passed to isc_put_segment
}
$blobId = ibase_blob_close($blob);
```

## Proposed Fix

Add safety checks to `_php_ibase_blob_add()` and `_php_ibase_blob_get()`:

### Fix for `_php_ibase_blob_add()`:

```c
int _php_ibase_blob_add(zval *string_arg, ibase_blob *ib_blob)
{
    zend_ulong put_cnt = 0, rem_cnt;
    unsigned short chunk_size;

    // Safety check: verify blob handle is valid
    if (!ib_blob || ib_blob->bl_handle.ptr == 0) {
        _php_ibase_module_error("BLOB handle is invalid or has been closed");
        return FAILURE;
    }

    convert_to_string_ex(string_arg);

    for (rem_cnt = Z_STRLEN_P(string_arg); rem_cnt > 0; rem_cnt -= chunk_size) {
        chunk_size = rem_cnt > USHRT_MAX ? USHRT_MAX : (unsigned short)rem_cnt;

        if (isc_put_segment(IB_STATUS, &ib_blob->bl_handle.blob, chunk_size, 
                           &Z_STRVAL_P(string_arg)[put_cnt])) {
            _php_ibase_error();
            return FAILURE;
        }
        put_cnt += chunk_size;
    }
    return SUCCESS;
}
```

### Fix for `_php_ibase_blob_get()`:

```c
int _php_ibase_blob_get(zval *return_value, ibase_blob *ib_blob, zend_ulong max_len)
{
    // Safety check: verify blob handle is valid
    if (!ib_blob || ib_blob->bl_handle.ptr == 0) {
        _php_ibase_module_error("BLOB handle is invalid or has been closed");
        return FAILURE;
    }

    if (ib_blob->bl_qd.gds_quad_high || ib_blob->bl_qd.gds_quad_low) {
        // ... existing code ...
    }
}
```

### Additional Checks for PHP Functions

In `PHP_FUNCTION(ibase_blob_add)` (line ~359), add a check after fetching the resource:

```c
PHP_FUNCTION(ibase_blob_add)
{
    // ... existing code ...
    
    ib_blob = (ibase_blob *)zend_fetch_resource_ex(blob_arg, NULL, le_blob);

    if (!ib_blob) {
        RETURN_FALSE;
    }

    // Add this safety check
    if (ib_blob->bl_handle.ptr == 0) {
        _php_ibase_module_error("BLOB handle is invalid or has been closed");
        RETURN_FALSE;
    }

    // ... rest of code ...
}
```

Similarly for `PHP_FUNCTION(ibase_blob_get)`.

## Test Cases

### 1. Test for crash after transaction commit (blob_segfault_after_commit.phpt)

```php
--TEST--
Bug: SIGSEGV when using blob_add after transaction commit/rollback
--FILE--
<?php
$db = ibase_connect($test_base);
$trans = ibase_trans($db);
$blob = ibase_blob_create($trans);

// Commit transaction (invalidates blob handle)
ibase_commit($trans);

// This should NOT crash - should return false
$result = @ibase_blob_add($blob, "test data");
var_dump($result === false);  // Expected: bool(true)
?>
--EXPECT--
bool(true)
```

### 2. Test for chunked stream writes (blob_stream_chunked_write.phpt)

Tests the doctrine-firebird-driver pattern with various data sizes.

## Impact

- **Severity**: High (causes process crash)
- **Workaround**: No effective PHP-level workaround exists. Users must carefully manage blob lifecycle to ensure blobs are closed before transaction commit/rollback.
- **Affected Users**: Any application using BLOBs with complex transaction lifecycles, particularly ORMs like Doctrine DBAL.

## Related

- Comment in `_php_ibase_free_blob()` acknowledges invalid handle scenario
- `repro_segfault_blob.phpt` tests related cleanup issues
- `use_after_free-*.phpt` tests related memory safety issues

## Reporter

- Michael Wegener (mw@satware.com)
- Discovered while investigating crashes in doctrine-firebird-driver test suite
- Date: 2025-12-01

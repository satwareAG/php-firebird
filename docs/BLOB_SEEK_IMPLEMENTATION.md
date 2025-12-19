# BLOB Seek Implementation Plan

## Overview

This document outlines the implementation plan for adding `fbird_blob_seek()` functionality to the php-firebird extension.

## Current State

The current BLOB implementation in `fbird_blobs.c`:
- Uses Firebird 3.0+ OO API (`fbb_*` wrappers)
- Stream wrapper has `NULL` for seek: `NULL, /* seek not supported for sequential blobs */`
- Supports stream BLOBs (type 1) which ARE seekable in Firebird

## Firebird API

### IBlob Interface (Firebird 3.0+)

The Firebird IBlob interface provides:
```cpp
int IBlob::seek(IStatus* status, int mode, int offset)
```

**Parameters:**
- `mode`: Seek mode
  - 0 = from start (SEEK_SET)
  - 1 = from current position (SEEK_CUR)
  - 2 = from end (SEEK_END)
- `offset`: Offset in bytes (can be negative for modes 1 and 2)

**Return:** New position in the blob, or -1 on error

**Note:** Only works for STREAM blobs (subtype 1), not segmented blobs (subtype 0).

## Implementation Steps

### Step 1: Add C Wrapper Function

In `firebird_utils.cpp` / `firebird_utils.h`:

```cpp
// Declaration
int fbb_seek(void* master, void* blob_handle, int mode, int offset, ISC_STATUS* status);

// Implementation
int fbb_seek(void* master, void* blob_handle, int mode, int offset, ISC_STATUS* status) {
    IBlob* blob = (IBlob*)block_handle;
    ThrowStatusWrapper status_wrapper(get_status(master));
    
    try {
        return blob->seek(&status_wrapper, mode, offset);
    } catch (FbException& e) {
        copy_status(status, e.getStatus());
        return -1;
    }
}
```

### Step 2: Add PHP Function

In `fbird_blobs.c`:

```c
/* {{{ proto int|false fbird_blob_seek(resource blob_handle, int offset [, int whence])
   Seek to a position in a stream blob */
PHP_FUNCTION(fbird_blob_seek)
{
    zval *blob_arg;
    zend_long offset;
    zend_long whence = FBIRD_BLOB_SEEK_SET;
    fbird_blob *ib_blob;

    RESET_ERRMSG;

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "rl|l", 
            &blob_arg, &offset, &whence)) {
        return;
    }

    ib_blob = (fbird_blob *)zend_fetch_resource_ex(blob_arg, LE_BLOB, le_blob);

    if (!ib_blob || !ib_blob->fbb_blob) {
        _php_fbird_module_error("Invalid BLOB handle");
        RETURN_FALSE;
    }

    // Validate whence parameter
    if (whence < 0 || whence > 2) {
        _php_fbird_module_error("Invalid seek whence: must be FBIRD_BLOB_SEEK_SET, "
            "FBIRD_BLOB_SEEK_CUR, or FBIRD_BLOB_SEEK_END");
        RETURN_FALSE;
    }

    int result = fbb_seek(IBG(master_instance), ib_blob->fbb_blob, 
        (int)whence, (int)offset, IB_STATUS);
    
    if (result < 0) {
        _php_fbird_error();
        RETURN_FALSE;
    }

    RETURN_LONG(result);
}
/* }}} */
```

### Step 3: Add Constants

In `firebird.c` MINIT:

```c
/* BLOB seek constants */
REGISTER_LONG_CONSTANT("FBIRD_BLOB_SEEK_SET", 0, CONST_PERSISTENT);
REGISTER_LONG_CONSTANT("FBIRD_BLOB_SEEK_CUR", 1, CONST_PERSISTENT);
REGISTER_LONG_CONSTANT("FBIRD_BLOB_SEEK_END", 2, CONST_PERSISTENT);
```

### Step 4: Add Stream Wrapper Seek (Optional Enhancement)

Modify `fbird_blob_stream_ops` to support seeking:

```c
static int fbird_blob_stream_seek(php_stream *stream, zend_off_t offset, int whence, zend_off_t *newoffset)
{
    fbird_blob_stream_data *data = (fbird_blob_stream_data *)stream->abstract;
    fbird_blob *ib_blob = data->ib_blob;

    if (!ib_blob || !ib_blob->fbb_blob) {
        return -1;
    }

    int result = fbb_seek(IBG(master_instance), ib_blob->fbb_blob, whence, (int)offset, IB_STATUS);
    
    if (result < 0) {
        return -1;
    }

    if (newoffset) {
        *newoffset = result;
    }
    return 0;
}

static const php_stream_ops fbird_blob_stream_ops = {
    fbird_blob_stream_write,
    fbird_blob_stream_read,
    fbird_blob_stream_close,
    fbird_blob_stream_flush,
    "fbird_blob",
    fbird_blob_stream_seek,  /* NOW ENABLED */
    NULL, /* cast */
    NULL, /* stat */
    NULL  /* set_option */
};
```

### Step 5: Register Function

In `firebird.c` function entries:

```c
PHP_FE(fbird_blob_seek, arginfo_fbird_blob_seek)
```

Add arginfo:

```c
ZEND_BEGIN_ARG_INFO_EX(arginfo_fbird_blob_seek, 0, 0, 2)
    ZEND_ARG_INFO(0, blob_handle)
    ZEND_ARG_INFO(0, offset)
    ZEND_ARG_INFO(0, whence)
ZEND_END_ARG_INFO()
```

## Test Cases

### tests/fbird_blob_seek_001.phpt

```php
--TEST--
fbird_blob_seek() - Seek in stream blob
--SKIPIF--
<?php include("skipif.inc"); ?>
--FILE--
<?php
require("firebird.inc");

$db = fbird_connect($test_base);
$trans = fbird_trans($db);

// Create a stream blob with known content
$content = str_repeat("ABCDEFGHIJ", 100); // 1000 bytes
$blob = fbird_blob_create($db, $trans);
fbird_blob_add($blob, $content);
$blob_id = fbird_blob_close($blob);

// Open blob for reading
$blob = fbird_blob_open($db, $trans, $blob_id);

// Test SEEK_SET
$pos = fbird_blob_seek($blob, 50, FBIRD_BLOB_SEEK_SET);
var_dump($pos === 50);
$data = fbird_blob_get($blob, 10);
var_dump($data === "ABCDEFGHIJ");

// Test SEEK_CUR (forward)
fbird_blob_seek($blob, 0, FBIRD_BLOB_SEEK_SET); // Reset
fbird_blob_get($blob, 20); // Read 20 bytes, now at pos 20
$pos = fbird_blob_seek($blob, 30, FBIRD_BLOB_SEEK_CUR); // Should be at 50
var_dump($pos === 50);

// Test SEEK_END
$pos = fbird_blob_seek($blob, -10, FBIRD_BLOB_SEEK_END);
var_dump($pos === 990);

fbird_blob_close($blob);
fbird_commit($trans);
fbird_close($db);

echo "Done.\n";
?>
--EXPECT--
bool(true)
string(10) "ABCDEFGHIJ"
bool(true)
bool(true)
Done.
```

## Limitations

1. **Only works with STREAM blobs** - Segmented blobs (subtype 0) cannot be seeked
2. **Firebird 3.0+ required** - Uses OO API's IBlob::seek()
3. **Negative offsets** - Only valid with SEEK_CUR and SEEK_END modes

## mlazdans/firebird-php Reference

Their implementation provides:
```php
public function seek(int $offset, int $mode): int|false;

// Constants
const BLOB_SEEK_START = 0;
const BLOB_SEEK_CURRENT = 1;
const BLOB_SEEK_END = 2;
```

## Compatibility Notes

- Our constant names use `FBIRD_BLOB_SEEK_*` prefix for consistency with other constants
- Return type matches: `int|false` (position on success, false on error)
- Parameters match standard PHP seek conventions (like `fseek()`)

## Implementation Priority

This is a C code change that requires:
1. Modifying `firebird_utils.cpp` (C++ wrapper layer)
2. Modifying `fbird_blobs.c` (PHP function)
3. Modifying `firebird.c` (constants and function registration)
4. Comprehensive testing

Estimated effort: Medium (2-4 hours for experienced C developer)

## See Also

- [docs/MLAZDANS_FIREBIRD_PHP_COMPARISON.md](MLAZDANS_FIREBIRD_PHP_COMPARISON.md) - Feature comparison
- Firebird IBlob interface documentation

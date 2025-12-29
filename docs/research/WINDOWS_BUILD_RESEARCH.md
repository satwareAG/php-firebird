# Windows Build Research for php-firebird

**Date:** 2025-12-29
**Purpose:** Document findings from research on PHP extension Windows builds

## Executive Summary

This document summarizes research on building PHP extensions for Windows, including analysis of the mlazdans/firebird-php project and official PHP Windows builder tools.

---

## 1. Key Findings from mlazdans/firebird-php

### 1.1 config.w32 Configuration

```javascript
// Uses fbclient_ms.lib - MSVC-compiled Firebird client
CHECK_LIB("fbclient_ms.lib", "firebird", PHP_FIREBIRD + "\\lib")

// Adds ZEND_ENABLE_STATIC_TSRMLS_CACHE define
EXTENSION("firebird", "...", PHP_FIREBIRD_SHARED, "/DZEND_ENABLE_STATIC_TSRMLS_CACHE=1");
```

**Key difference from our config.w32:**
- They use `fbclient_ms.lib` (MSVC version) not `fbclient.lib`
- They pass `/DZEND_ENABLE_STATIC_TSRMLS_CACHE=1` flag

### 1.2 C++ and PHP Header Handling

**firebird_php.hpp** (Header file):
```cpp
extern "C" {
#include "php.h"

// ALL PHP declarations inside extern "C"
extern zend_module_entry firebird_module_entry;

ZEND_BEGIN_MODULE_GLOBALS(firebird)
    // Module globals here
ZEND_END_MODULE_GLOBALS(firebird)

#if defined(ZTS) && defined(COMPILE_DL_FIREBIRD)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

} // end extern "C"
```

**firebird_utils.cpp** (C++ source):
```cpp
// C++ headers first
#include <firebird/Interface.h>
#include <iostream>
#include <string>

// Internal C++ headers
#include "fbp/base.hpp"
#include "firebird_php.hpp"

// PHP headers wrapped in extern "C"
extern "C" {
#include "ibase.h"
#include "php.h"
#include "zend_exceptions.h"
#include "ext/spl/spl_exceptions.h"
#include "ext/date/php_date.h"
#include "firebird_utils.h"
}

// C++ code here...

// All C functions callable from PHP wrapped at the end
extern "C" {
int fbu_database_connect(size_t dbh, zval *args) { ... }
// ... all fbu_* functions
}
```

### 1.3 Release Artifact Naming

From v0.1.0a release:
```
php_firebird-0.1.0a-8.4-vs17-nts-x86_64.dll  # PHP 8.4, VS2022, NTS, 64-bit
php_firebird-0.1.0a-8.4-vs17-nts.dll         # PHP 8.4, VS2022, NTS, 32-bit
php_firebird-0.1.0a-8.4-vs17-x86_64.dll      # PHP 8.4, VS2022, TS, 64-bit
php_firebird-0.1.0a-8.4-vs17.dll             # PHP 8.4, VS2022, TS, 32-bit
```

**Naming Convention:** `php_<ext>-<ver>-<php_ver>-<vs_ver>[-nts][-x86_64].dll`

### 1.4 Build Process

They use local batch scripts with the official PHP SDK:
- `phpsdk-vs*.bat` - Visual Studio environment setup
- `phpize` → `configure` → `nmake` workflow
- No GitHub Actions workflow (manual builds)

---

## 2. Official php-windows-builder GitHub Action

### 2.1 Repository

`php/php-windows-builder` - Official PHP Windows build tools

### 2.2 Extension Build Workflow

```yaml
name: Build Windows Extension
on: [push, pull_request]

jobs:
  get-extension-matrix:
    runs-on: ubuntu-latest
    outputs:
      matrix: ${{ steps.extension-matrix.outputs.matrix }}
    steps:
      - uses: actions/checkout@v5
      - id: extension-matrix
        uses: php/php-windows-builder/extension-matrix@v1
        with:
          php-version-list: '8.2, 8.3, 8.4'
          allow_old_php_versions: false

  build:
    needs: get-extension-matrix
    runs-on: ${{ matrix.os }}
    strategy:
      matrix: ${{ fromJson(needs.get-extension-matrix.outputs.matrix) }}
    steps:
      - uses: actions/checkout@v5
      - uses: php/php-windows-builder/build-php-extension@v1
        with:
          php-version: ${{ matrix.php-version }}
          arch: ${{ matrix.arch }}
          ts: ${{ matrix.ts }}
          args: --with-firebird=/path/to/firebird
```

### 2.3 Required Inputs

| Input | Description |
|-------|-------------|
| `php-version` | PHP version to target (required) |
| `arch` | x64 or x86 (required) |
| `ts` | NTS or TS (required) |
| `extension-url` | Git repo URL (optional, defaults to current) |
| `extension-ref` | Git ref/branch (optional) |
| `args` | Extra configure args (optional) |

### 2.4 Benefits Over Manual Approach

1. **Automatic MSVC/PHP version matching** - No manual VS version management
2. **Matrix generation** - Automatically creates build matrix for all supported PHP versions
3. **Official tooling** - Maintained by PHP project
4. **CI/CD integration** - Works in GitHub Actions workflows

---

## 3. Key MSVC/Windows Compatibility Issues

### 3.1 extern "C" Requirements

**Problem:** TSRM symbols (`_tsrm_ls_cache`) must have C linkage for proper DLL export.

**Solution:** Wrap ALL PHP headers in `extern "C"` blocks:

```cpp
// In .cpp files
extern "C" {
#include "php.h"
#include "php_fbird_includes.h"
}

// In header files
#if defined(ZTS) && defined(COMPILE_DL_FIREBIRD)
#ifdef __cplusplus
extern "C" {
#endif
ZEND_TSRMLS_CACHE_EXTERN()
#ifdef __cplusplus
}
#endif
#endif
```

### 3.2 Inline Functions

**Problem:** MSVC doesn't export `inline` functions in DLLs.

**Solution:** Remove `inline` keyword from functions declared as `extern "C"`:

```cpp
// WRONG
extern "C" {
inline void fbsvc_detach(...) { ... }
}

// CORRECT
extern "C" {
void fbsvc_detach(...) { ... }
}
```

### 3.3 void* Pointer Arithmetic

**Problem:** GCC allows void* arithmetic as extension, MSVC does not.

**Solution:** Cast to `char*` before arithmetic:

```c
// WRONG (GCC extension)
result = base + offset;  // base is void*

// CORRECT (Standard C)
result = (char*)base + offset;
```

### 3.4 POSIX Functions

| POSIX | Windows Equivalent |
|-------|-------------------|
| `setenv()` | `_putenv_s()` |
| `unsetenv()` | `_putenv_s(name, "")` |
| `pthread.h` | `CRITICAL_SECTION` / `SRWLock` |
| `execinfo.h` | Not available (no backtrace) |

### 3.5 fbclient Library

| File | Description |
|------|-------------|
| `fbclient.lib` | Import library (MinGW/generic) |
| `fbclient_ms.lib` | Import library (MSVC) |
| `fbclient.dll` | Firebird client DLL |

**Firebird SDK locations:**
- Headers: `$FIREBIRD/include/`
- Libraries: `$FIREBIRD/lib/`
- Runtime DLL: `$FIREBIRD/fbclient.dll`

---

## 4. Recommendations for php-firebird

### 4.1 Short-term Fixes (Current Approach)

1. ✅ Wrap PHP headers in `extern "C"` in C++ files
2. ✅ Wrap `ZEND_TSRMLS_CACHE_EXTERN` in `extern "C"` in headers
3. ✅ Remove `inline` from extern C functions
4. ✅ Fix void* pointer arithmetic
5. ✅ Add Windows stubs for POSIX functions

### 4.2 Medium-term: Adopt php-windows-builder

Switch to official `php/php-windows-builder` GitHub Action:

```yaml
# .github/workflows/release-windows.yml
name: Build Windows DLLs

on:
  push:
    tags: ['v*']

jobs:
  get-matrix:
    runs-on: ubuntu-latest
    outputs:
      matrix: ${{ steps.ext-matrix.outputs.matrix }}
    steps:
      - uses: actions/checkout@v5
      - id: ext-matrix
        uses: php/php-windows-builder/extension-matrix@v1
        with:
          php-version-list: '8.2, 8.3, 8.4'

  build:
    needs: get-matrix
    runs-on: ${{ matrix.os }}
    strategy:
      matrix: ${{ fromJson(needs.get-matrix.outputs.matrix) }}
    steps:
      - uses: actions/checkout@v5
      
      - name: Download Firebird SDK
        run: |
          # Download and extract Firebird SDK
          
      - uses: php/php-windows-builder/build-php-extension@v1
        with:
          php-version: ${{ matrix.php-version }}
          arch: ${{ matrix.arch }}
          ts: ${{ matrix.ts }}
          args: --with-firebird=${{ github.workspace }}/firebird-sdk

      - uses: actions/upload-artifact@v4
        with:
          name: php_firebird-${{ matrix.php-version }}-${{ matrix.arch }}-${{ matrix.ts }}
          path: '*.dll'
```

### 4.3 config.w32 Updates

Consider adding:
1. `/DZEND_ENABLE_STATIC_TSRMLS_CACHE=1` compiler flag
2. Support for `fbclient_ms.lib` as alternative to `fbclient.lib`

---

## 5. References

- [php/php-windows-builder](https://github.com/php/php-windows-builder)
- [mlazdans/firebird-php](https://github.com/mlazdans/firebird-php)
- [shivammathur/php-builder-windows](https://github.com/shivammathur/php-builder-windows)
- [PHP Windows Build Environment](https://wiki.php.net/internals/windows/stepbystepbuild_sdk_2)

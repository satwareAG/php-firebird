# Cleanup and Windows Development Strategy

## Executive Summary

This document identifies outdated code, documentation, and build configurations that need cleanup, plus outlines a strategy for Windows development and testing from a Linux host without licensing costs.

**PHP 8.1+ Minimum Requirement**: Since the extension now requires PHP 8.1+, all compatibility code for PHP 7.x and 8.0 should be removed.

**Breaking Change Policy**: "BC must not be cared for. It is good if it breaks so people do know it is a new driver without interbase roots"

---

## Part 1: Cleanup Areas Identified

### 1.1 Critical: Test Files Using Old Function Names

**File: `tests/functions.inc`**
- **Status**: ✅ COMPLETE (Phase 2, commit 8d33a6c)
- **Changes Made**:
  - Replaced 17 `ibase_*` calls with `fbird_*`
  - Renamed `ibase_query_bulk()` → `fbird_query_bulk()`
  - Renamed `php_ibase_exception_handler()` → `php_fbird_exception_handler()`

**File: `tests/common.inc`**
- **Status**: ✅ COMPLETE (Phase 2, commit 8d33a6c)
- **Changes Made**:
  - Replaced 37 `ibase_*` calls with `fbird_*`
  - Renamed `test_use_after_ibase_free_query()` → `test_use_after_fbird_free_query()`
  - Renamed `test_ibase_trans_014_015()` → `test_fbird_trans_014_015()`

**Updated Test Files** (8 files referencing renamed utility functions):
- fbird_close_004.phpt, fbird_close_005.phpt
- fbird_trans_008.phpt, fbird_trans_009.phpt
- fbird_trans_014.phpt, fbird_trans_015.phpt
- use_after_free-001.phpt, use_after_free-002.phpt

### 1.2 Obsolete: PHP Version Skip File

**File: `tests/skipif-php80-or-older.inc`**
- **Status**: ✅ COMPLETE (Phase 1, commit eb3f9f4)
- **Changes Made**: 
  1. Deleted `tests/skipif-php80-or-older.inc`
  2. Removed `include("skipif-php80-or-older.inc");` from 12 test files
  3. Removed `if(PHP_MAJOR_VERSION < 8)` from use_after_free-002.phpt

**Affected Test Files**:
- `tests/bug46247_003.phpt`
- `tests/bug46247_004.phpt`
- `tests/fbird_close_003.phpt`
- `tests/fbird_close_005.phpt`
- `tests/fbird_drop_db_003.phpt`
- `tests/fbird_drop_db_004.phpt`
- `tests/fbird_free_query_002.phpt`
- `tests/fbird_num_fields_003.phpt`
- `tests/fbird_num_fields_004.phpt`
- `tests/fbird_num_params_003.phpt`
- `tests/fbird_num_params_004.phpt`
- `tests/fbird_param_info_003.phpt`

### 1.3 Windows Build Scripts - Outdated

**Directory: `scripts/host/windows/`**
- **Status**: ✅ COMPLETE (December 2025)

| File | Issues | Status |
|------|--------|--------|
| `php-fb-build-all.bat` | References PHP 7.4.13 and 8.0.30 | ✅ Updated to PHP 8.1+ only |
| `php-fb-build.bat` | Uses old `interbase` naming throughout | ✅ Updated to `firebird` naming |
| `php-fb-sdk-init.bat` | Has PHP 7.3 specific code | ⚠️ Review if still needed |
| `php-fb-sdk-build.bat` | Likely uses old naming | ⚠️ Review if still needed |
| `php-fb-config.dist.bat` | May have outdated references | ⚠️ Review |

**Changes Made**:
- `php-fb-build-all.bat`: Removed PHP 7.4/8.0 entries, now PHP 8.1+ only
- `php-fb-build.bat`: Updated all `interbase` → `firebird` naming, fixed header file path

### 1.4 Docker Release Build File - Outdated

**File: `docker/Dockerfile`**
- **Status**: ✅ COMPLETE (December 2025)
- **Changes Made**:
  - Removed PHP 7.4.13 and 8.0.30, now PHP 8.1+ only
  - Changed `--with-interbase` to `--with-firebird`
  - Changed `interbase.so` to `firebird.so`
  - Changed clone source to satwareAG/php-firebird
  - Added branch argument for flexibility

### 1.5 GitHub Actions Workflow - Outdated Naming

**File: `.github/workflows/main.yml`**
- **Status**: ✅ COMPLETE (December 2025, commit 9e9e20a)
- **Changes Made**:
  - Changed `--with-interbase=/opt/firebird` to `--with-firebird=/opt/firebird`
  - Updated comment from `tests/interbase.inc` to `tests/firebird.inc`
  - Updated build step name to "Build PHP extension (firebird)"
- **Future**: Windows CI can be added later using GitHub Actions

### 1.6 Documentation - Old Naming Throughout

**File: `docs/deployment/CROSS_PLATFORM_DEPLOYMENT.md`**
- **Status**: ✅ COMPLETE (December 2025)
- **Changes Made**:
  - Complete rewrite with `fbird_*` function naming
  - All examples now use `firebird.so` and `extension=firebird`
  - Updated PHP examples to use `fbird_connect()`, `fbird_close()`, etc.
  - Added migration section linking to README.md
  - Updated Docker examples with correct naming

### 1.7 Build Infrastructure - PHP Version Compatibility

**File: `build/gen_stub.php`**
- **Status**: ⚠️ Contains PHP 7.0/8.0 compatibility code
- **Context**: This is likely a copy from PHP source for generating stubs
- **Action**: Review if needed; may be safe to update minimum version constants

**File: `build/php.m4`**
- **Status**: Contains PHP 7.4 minimum check for gen_stub.php
- **Action**: Update minimum version comment/check

---

## Part 2: Windows Development Strategy

### 2.1 Modern PHP Windows Extension Development Requirements

**PHP 8.1+ Windows Extension Requirements**:

| Component | Requirement | Source |
|-----------|-------------|--------|
| **Visual Studio** | VS 2019 (vs16) for PHP 8.1-8.3, VS 2022 (vs17) for PHP 8.4+ | php.net/downloads |
| **PHP SDK** | php-sdk-binary-tools | github.com/php/php-sdk-binary-tools |
| **Architecture** | x64 primary (x86 deprecated) | php.net/downloads |
| **Thread Safety** | Both TS and NTS builds | php.net/downloads |
| **C++ Standard** | C++17 (MSVC 2017+) | Extension requirement |
| **Firebird Client** | Firebird 4.0+ Windows Client Libraries | firebirdsql.org |

### 2.2 Free Windows Testing Options

#### Option 1: GitHub Actions Windows Runners (RECOMMENDED)

**Cost**: FREE for public repositories
**Platform**: Windows Server 2019/2022 with VS pre-installed
**PHP**: Available via shivammathur/setup-php action

**Proposed Workflow** (`.github/workflows/windows.yml`):
```yaml
name: Windows Build

on: [push, pull_request]

jobs:
  windows-build:
    runs-on: windows-latest
    strategy:
      matrix:
        php: ['8.1', '8.2', '8.3', '8.4']
        arch: [x64]
        ts: [ts, nts]
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Setup PHP
        uses: shivammathur/setup-php@v2
        with:
          php-version: ${{ matrix.php }}
          extensions: none
          ini-values: extension_dir=ext
          tools: phpize
      
      - name: Download Firebird Client
        run: |
          Invoke-WebRequest -Uri "https://github.com/FirebirdSQL/firebird/releases/download/v5.0.3/Firebird-5.0.3.1683-ReleaseCandidate1-windows-x64.zip" -OutFile "firebird.zip"
          Expand-Archive -Path firebird.zip -DestinationPath C:\firebird
      
      - name: Setup PHP SDK
        run: |
          Invoke-WebRequest -Uri "https://github.com/php/php-sdk-binary-tools/releases/download/php-sdk-2.3.0/php-sdk-binary-tools-php-sdk-2.3.0.zip" -OutFile sdk.zip
          Expand-Archive -Path sdk.zip -DestinationPath C:\php-sdk
      
      - name: Build Extension
        shell: cmd
        run: |
          call C:\php-sdk\phpsdk-vs17-x64.bat
          phpize
          configure --with-firebird=C:\firebird
          nmake
      
      - name: Test Extension
        run: |
          php -d extension=Release\php_firebird.dll -m | findstr firebird
```

#### Option 2: Azure Pipelines

**Cost**: FREE tier (1800 minutes/month for public projects)
**Platform**: Windows Server with VS
**Advantage**: Same Microsoft infrastructure as GitHub Actions

#### Option 3: Wine-Based Testing (LIMITED)

**Cost**: FREE
**Platform**: Linux with Wine
**Limitations**: 
- Cannot build Windows DLLs (no MSVC)
- Can only test pre-built DLLs
- Limited compatibility

#### Option 4: Windows VM Evaluation

**Cost**: FREE for 90 days
**Source**: developer.microsoft.com/windows/downloads/virtual-machines
**Platforms**: VirtualBox, VMware, Hyper-V, Parallels
**Use Case**: Local development/debugging

### 2.3 Cross-Compilation Limitations

**Cannot cross-compile PHP extensions from Linux to Windows because:**
1. PHP Windows extensions require MSVC toolchain (not MinGW compatible)
2. PHP's Windows build system uses Visual Studio project files
3. Firebird Windows client libraries are MSVC-compiled

**Best Practice**: Use GitHub Actions for Windows CI/CD

### 2.4 Required config.w32 File

The project is missing `config.w32` for Windows builds. This needs to be created:

```javascript
// config.w32 - Windows build configuration
ARG_WITH('firebird', 'Firebird support', 'no');

if (PHP_FIREBIRD != 'no') {
    if (CHECK_LIB('fbclient_ms.lib', 'firebird', PHP_FIREBIRD + '\\lib') &&
        CHECK_HEADER_ADD_INCLUDE('ibase.h', 'CFLAGS_FIREBIRD', PHP_FIREBIRD + '\\include')) {
        
        EXTENSION('firebird', 'firebird.c fbird_blobs.c fbird_events.c fbird_inspection.c ' +
                              'fbird_metadata.c fbird_query.c fbird_query_exec.c fbird_result.c ' +
                              'fbird_service.c fbird_udf.c firebird_utils.cpp', 
                  PHP_FIREBIRD_SHARED, '/DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 /EHsc');
        
        AC_DEFINE('HAVE_FIREBIRD', 1, 'Have Firebird support');
        
        // Copy Firebird client DLL to build output
        ADD_FLAG('DEPS_FIREBIRD', 'fbclient.dll');
    } else {
        WARNING('firebird not enabled; libraries and headers not found');
    }
}
```

---

## Part 3: Recommended Cleanup Order

### Phase 1: Critical Test Infrastructure (Must Be First)

1. **Update `tests/functions.inc`**: Replace all `ibase_*` with `fbird_*`
2. **Remove `tests/skipif-php80-or-older.inc`**: Delete file and remove includes
3. **Update any remaining test files**: Search for `ibase_` usage

### Phase 2: Windows Build Scripts

1. **Create `config.w32`**: New file for Windows builds
2. **Update `php-fb-build-all.bat`**: Remove PHP 7.4/8.0
3. **Update `php-fb-build.bat`**: Change interbase→firebird naming
4. **Update `php-fb-sdk-init.bat`**: Remove PHP 7.3 handling
5. **Create `.github/workflows/windows.yml`**: Add Windows CI

### Phase 3: GitHub Actions

1. **Update `.github/workflows/main.yml`**: 
   - Change `--with-interbase` to `--with-firebird`
   - Update comments

### Phase 4: Docker Release Build

1. **Rewrite `docker/Dockerfile`**: 
   - Remove PHP 7.4/8.0
   - Update all naming
   - Clone from satwareAG/php-firebird

### Phase 5: Documentation

1. **Rewrite `docs/deployment/CROSS_PLATFORM_DEPLOYMENT.md`**: All new naming
2. **Review all docs**: Search for remaining `interbase` or `ibase_` references

### Phase 6: Build Infrastructure

1. **Review `build/gen_stub.php`**: Update if needed
2. **Update `build/php.m4`**: Update minimum version references

---

## Part 4: Verification Commands

### Find All Remaining Old References

```bash
# Find ibase_ function usage
grep -r "ibase_" --include="*.php" --include="*.phpt" tests/

# Find interbase references in scripts
grep -ri "interbase" scripts/

# Find interbase in documentation
grep -ri "interbase" docs/

# Find PHP version checks for < 8.1
grep -rE "PHP_VERSION_ID.*8010|PHP_MAJOR_VERSION.*[7-8]" .

# Find old configure flags
grep -r "with-interbase" .
```

### Validate After Cleanup

```bash
# Rebuild extension
docker exec php-firebird-dev-php83-dev-1 sh -c "cd /ext && phpize --clean && phpize && ./configure && make clean && make -j\$(nproc)"

# Run all tests
docker exec php-firebird-dev-php83-dev-1 sh -c "cd /ext && make test TESTS='-q --show-diff tests/'"
```

---

## Summary - Completion Status (December 2025)

| Category | Items | Status |
|----------|-------|--------|
| Test Infrastructure | functions.inc, common.inc | ✅ COMPLETE |
| Windows Scripts | php-fb-build-all.bat, php-fb-build.bat | ✅ COMPLETE |
| Windows Build Config | config.w32 (NEW) | ✅ CREATED |
| GitHub Actions | main.yml | ✅ COMPLETE |
| Docker Files | Dockerfile | ✅ COMPLETE |
| Container Scripts | build.sh | ✅ COMPLETE |
| Documentation | CROSS_PLATFORM_DEPLOYMENT.md | ✅ COMPLETE |
| Build Infrastructure | gen_stub.php, php.m4 | ⚠️ LOW PRIORITY |

**Completed Files**: 9 files updated/created
**Remaining**: 2 build infrastructure files (low priority), 3 Windows scripts (review needed)

**Next Steps**:
1. Test static analysis with updated clang-tidy configuration
2. Optionally add Windows CI workflow (`.github/workflows/windows.yml`)
3. Review remaining Windows scripts (`php-fb-sdk-*.bat`, `php-fb-config.dist.bat`)

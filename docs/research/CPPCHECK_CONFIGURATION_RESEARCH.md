# Cppcheck Configuration Research and 2025 Alternatives

**Date:** 2025-12-21  
**Project:** php-firebird v7.0.0-rc.1  
**Author:** Research for cppcheck false positive elimination

## Executive Summary

The cppcheck warnings for `staticFunction` and `unusedFunction` in `fbird_datetime.c` are **confirmed false positives** caused by single-file analysis limitations. This document provides:

1. **Root Cause Analysis** - Why cppcheck produces false positives
2. **Configuration Solutions** - Methods to eliminate false positives
3. **2025 Tool Comparison** - Alternative C/C++ static analysis tools
4. **Recommendations** - Best approach for php-firebird project

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Root Cause Analysis](#root-cause-analysis)
3. [Solution Options](#solution-options)
   - [Option A: Suppression File](#option-a-suppression-file-recommended)
   - [Option B: Inline Suppressions](#option-b-inline-suppressions)
   - [Option C: Compilation Database](#option-c-compilation-database)
   - [Option D: CTU Analysis](#option-d-ctu-analysis)
4. [2025 Static Analysis Tools Comparison](#2025-static-analysis-tools-comparison)
5. [Recommendations](#recommendations)
6. [Implementation](#implementation)

---

## Problem Statement

### Observed Warnings

```
fbird_datetime.c:139:0: style: The function 'fbird_parse_timestamp' is never used. [unusedFunction]
fbird_datetime.c:50:0: style: The function 'fbird_datetime_init' should have static linkage... [staticFunction]
fbird_datetime.c:270:0: style: The function 'fbird_parse_date' should have static linkage... [staticFunction]
fbird_datetime.c:336:0: style: The function 'fbird_parse_time' should have static linkage... [staticFunction]
```

### Proof These Are False Positives

When attempting to make these functions `static`, compilation fails:

```
/ext/fbird_query_bind.c:696:66: error: implicit declaration of function 'fbird_parse_timestamp'
/ext/firebird.c:3214:66: error: implicit declaration of function 'fbird_parse_timestamp'
/ext/fbird_query_bind.c:738:41: error: implicit declaration of function 'fbird_datetime_init'
/ext/firebird.c:3233:33: error: implicit declaration of function 'fbird_datetime_init'
```

### Functions Actually Used Cross-File

| Function | Declared In | Used In | Lines |
|----------|-------------|---------|-------|
| `fbird_datetime_init` | fbird_datetime.h | firebird.c | 3233 |
| | | fbird_query_bind.c | 738 |
| `fbird_parse_timestamp` | fbird_datetime.h | firebird.c | 3214 |
| | | fbird_query_bind.c | 696 |
| `fbird_parse_date` | fbird_datetime.h | (public API) | - |
| `fbird_parse_time` | fbird_datetime.h | (public API) | - |

---

## Root Cause Analysis

### Why Cppcheck Produces False Positives

1. **Single-File Analysis Mode (Default)**
   - Cppcheck analyzes each `.c` file independently
   - Cannot see cross-file function usage
   - Recommends `static` for functions not called within the same file

2. **No Header Awareness**
   - Does not automatically understand that functions declared in headers are public API
   - Treats all non-called functions as candidates for `static`

3. **Current Configuration Issues**
   - `.cppcheck` file has `disable=unusedFunction` but still reports `staticFunction`
   - Configuration syntax may not be properly recognized

### Current `.cppcheck` Analysis

```ini
# Current configuration (partial)
enable=warning,performance,portability,information,missingInclude
disable=unusedFunction,missingIncludeSystem
```

**Issue:** The `disable=unusedFunction` should work, but `staticFunction` is not disabled. The `staticFunction` check is part of the `style` category.

---

## Solution Options

### Option A: Suppression File (RECOMMENDED)

Create a dedicated suppressions file with specific rules.

**File: `.cppcheck-suppressions`**
```
# Suppress false positives for cross-file functions
# These functions are declared in fbird_datetime.h and used in other translation units

# staticFunction - functions declared in headers are intentionally non-static
staticFunction:fbird_datetime.c:50
staticFunction:fbird_datetime.c:270
staticFunction:fbird_datetime.c:336

# unusedFunction - functions used in firebird.c and fbird_query_bind.c
unusedFunction:fbird_datetime.c:139

# Alternative: suppress all staticFunction warnings in datetime module
# staticFunction:fbird_datetime.c
```

**Usage:**
```bash
cppcheck --suppressions-list=.cppcheck-suppressions --enable=all .
```

**Pros:**
- Explicit documentation of known false positives
- No code changes required
- Easy to maintain and review
- Git-trackable

**Cons:**
- Requires line number updates if code changes significantly


### Option B: Inline Suppressions

Add comments directly in source code.

**In `fbird_datetime.c`:**
```c
// cppcheck-suppress staticFunction
void fbird_datetime_init(fbird_datetime_components* out) {
    // ...
}

// cppcheck-suppress unusedFunction
int fbird_parse_timestamp(const char* str, fbird_datetime_components* out) {
    // ...
}
```

**Alternative: File-level suppression at top of file:**
```c
// cppcheck-suppress-file staticFunction
// cppcheck-suppress-file unusedFunction
```

**Pros:**
- Suppression lives with the code
- Self-documenting
- Survives line number changes

**Cons:**
- Pollutes source code with tool-specific comments
- Requires `--inline-suppr` flag (already enabled in current config)


### Option C: Compilation Database

Generate `compile_commands.json` for whole-program analysis.

**For PHP Extensions (autoconf/make):**

1. **Install compiledb:**
   ```bash
   pip install compiledb
   ```

2. **Generate database during build:**
   ```bash
   # Inside Docker container or build environment
   phpize
   ./configure --with-firebird=/path/to/firebird
   compiledb make -j8
   ```

3. **Run cppcheck with project:**
   ```bash
   cppcheck --project=compile_commands.json --cppcheck-build-dir=.cppcheck-cache
   ```

**Alternative: Using bear:**
```bash
# Install bear
sudo apt install bear  # Debian/Ubuntu

# Generate compile_commands.json
bear -- make -j8
```

**Pros:**
- Full project context for accurate analysis
- Enables whole-program checks
- Picks up exact compiler flags

**Cons:**
- Requires build system integration
- More complex CI setup
- `compile_commands.json` needs regeneration on Makefile changes


### Option D: CTU (Cross-Translation-Unit) Analysis

Enable cppcheck's experimental CTU mode.

**Workflow:**
```bash
# Step 1: Generate CTU info files
for f in *.c; do
    cppcheck --dump --ctu-info "$f"
done

# Step 2: Create file list
ls *.ctu-info > ctu-filelist.txt

# Step 3: Run CTU analysis
cppcheck --ctu-filelist=ctu-filelist.txt .
```

**Pros:**
- True cross-file analysis
- Catches more bugs

**Cons:**
- Experimental feature
- Complex setup
- May have false positives of its own
- Not widely adopted

---

## 2025 Static Analysis Tools Comparison

### Overview Table

| Tool | Type | Cross-File | PHP Ext Support | False Positive Rate | Cost | Best For |
|------|------|------------|-----------------|---------------------|------|----------|
| **Cppcheck** | OSS | Limited | Good | Low | Free | Quick scans, CI |
| **Clang Static Analyzer** | OSS | Via CTU | Excellent | Low | Free | Deep analysis |
| **PVS-Studio** | Commercial | Yes | Good | Very Low | $$$ | Enterprise |
| **SonarQube** | Hybrid | Yes | Via plugin | Medium | Free/$$$ | Multi-lang |
| **Coverity** | Commercial | Yes | Good | Very Low | $$$$ | Compliance |
| **Infer (Meta)** | OSS | Yes | Limited | Low | Free | Memory issues |
| **CodeChecker** | OSS | Yes | Good | Low | Free | CI/CD integration |

### Detailed Analysis

#### 1. Cppcheck (Current)

**Version:** 2.x (2024-2025)  
**Website:** https://cppcheck.sourceforge.io/

**Strengths:**
- Lightweight, low false positives (when configured correctly)
- Easy setup, no compilation required
- Good for quick CI checks
- Mature tooling

**Weaknesses:**
- Single-file analysis by default
- CTU support is experimental
- No path-sensitive analysis

**Recommendation:** Keep as primary tool with proper suppression configuration.


#### 2. Clang Static Analyzer

**Website:** https://clang-analyzer.llvm.org/

**Strengths:**
- Path-sensitive analysis
- Excellent C/C++ support
- Integrated with LLVM/Clang toolchain
- CTU analysis available via `scan-build`
- Free and actively maintained

**Weaknesses:**
- Requires compilation
- Slower than cppcheck
- Limited to C/C++/Objective-C

**Usage for PHP Extensions:**
```bash
# Using scan-build wrapper
scan-build make

# Or with compile_commands.json
clang-tidy -p . *.c
```

**Recommendation:** Consider as secondary analyzer for deeper analysis.


#### 3. CodeChecker

**Website:** https://codechecker.readthedocs.io/

**Strengths:**
- Integrates Clang SA and Cppcheck
- Uses `compile_commands.json` natively
- Excellent CI/CD integration
- Web UI for results
- Free and open source

**Weaknesses:**
- More complex setup
- Requires Python environment

**Usage:**
```bash
# Install
pip install codechecker

# Analyze with both Clang SA and Cppcheck
CodeChecker analyze --analyzers=clangsa,cppcheck compile_commands.json -o reports

# Generate HTML report
CodeChecker parse reports -e html -o html_report
```

**Recommendation:** Excellent choice if migrating to compilation database approach.


#### 4. Clang-Tidy (Recommended Addition)

**Website:** Part of LLVM project

**Strengths:**
- Modernization checks (C++11/14/17/20)
- Custom checks via `.clang-tidy` (already present in project!)
- Uses `compile_commands.json`
- Auto-fix capability

**Current Project Config (`.clang-tidy`):**
```yaml
# Already configured in php-firebird!
```

**Recommendation:** Already configured - leverage existing `.clang-tidy` file.


#### 5. PVS-Studio (Commercial)

**Website:** https://pvs-studio.com/

**Strengths:**
- Exceptional bug detection
- Low false positive rate
- Excellent copy-paste detection
- CI integration

**Weaknesses:**
- Commercial license required
- May be overkill for smaller projects

**Recommendation:** Consider for client/enterprise projects with budget.


#### 6. Infer (Meta/Facebook)

**Website:** https://fbinfer.com/

**Strengths:**
- Excellent for memory issues (null pointers, leaks)
- Interprocedural analysis
- Free and open source

**Weaknesses:**
- Steeper learning curve
- Less mainstream adoption

**Recommendation:** Consider for memory-intensive codebases.

---

## Recommendations

### Immediate Action (Low Effort)

**Create `.cppcheck-suppressions` file** to eliminate known false positives:

```
# False positives: functions declared in fbird_datetime.h, used cross-file
staticFunction:fbird_datetime.c:50
staticFunction:fbird_datetime.c:270  
staticFunction:fbird_datetime.c:336
unusedFunction:fbird_datetime.c:139
```

**Update cppcheck invocation:**
```bash
cppcheck --suppressions-list=.cppcheck-suppressions \
         --enable=warning,performance,portability \
         --std=c17 --std=c++17 \
         --inline-suppr \
         .
```

### Medium-Term Improvement ✅ IMPLEMENTED

**Generate `compile_commands.json`** for whole-program analysis:

**Scripts Created:**
- `scripts/analysis/generate_compdb.sh` - Generates `compile_commands.json` using `bear` or `compiledb`
- `scripts/analysis/cppcheck.sh` - Enhanced to use `compile_commands.json` when available
- `scripts/analysis/clang_tidy.sh` - Enhanced with all 15 source files

**Docker Images Updated:**
- All Dockerfiles now include `bear` and `python3-pip` for compilation database generation

**Usage:**
```bash
# Inside Docker container
./scripts/analysis/generate_compdb.sh  # Generate compile_commands.json
./scripts/analysis/cppcheck.sh         # Run cppcheck with full context
./scripts/analysis/clang_tidy.sh       # Run clang-tidy analysis
```

### Long-Term Enhancement ✅ IMPLEMENTED

**Clang-Tidy CI Integration** (already have `.clang-tidy` config):

The `clang_tidy.sh` script now:
- Auto-generates `compile_commands.json` if missing (using `bear`)
- Analyzes all 15 C/C++ source files
- Uses proper include paths for PHP extension development
- Provides per-file pass/fail status

### Tool Combination Strategy

For comprehensive coverage, use multiple tools:

| Tool | Purpose | When to Run |
|------|---------|-------------|
| **Cppcheck** | Quick style/bug checks | Every commit |
| **Clang-Tidy** | Modernization, deeper checks | PR/merge |
| **Clang Static Analyzer** | Path-sensitive analysis | Release prep |

---

## Implementation

### Files to Create/Modify

1. **Create: `.cppcheck-suppressions`**
   - Suppress known false positives

2. **Update: `.cppcheck`**
   - Fix configuration syntax
   - Add suppression file reference

3. **Optional: CI integration**
   - Add `compiledb` generation step
   - Run cppcheck with project file

### Updated `.cppcheck` Configuration

```ini
# Cppcheck configuration for php-firebird
# Optimized for multi-file PHP extension analysis

[cppcheck]
# Language standards
std=c17
std=c++17

# Include paths (adjust per environment)
includePath=/usr/include/php
includePath=/usr/include/firebird
includePath=.

# Defines
define=FB_API_VER=40
define=HAVE_CONFIG_H

# Enable useful checks (avoid 'all' to reduce noise)
enable=warning
enable=performance  
enable=portability
enable=information

# Disable problematic checks for multi-file projects
# staticFunction and unusedFunction don't work correctly
# without whole-program analysis
suppress=staticFunction
suppress=unusedFunction
suppress=missingIncludeSystem

# Use external suppressions file for specific cases
suppressions-list=.cppcheck-suppressions

# Other options
inline-suppr=true
error-exitcode=2
checks-max-time=300
```

---

## Conclusion

The false positives are caused by cppcheck's inherent single-file analysis limitation. The **recommended approach** is:

1. **Immediate:** Create suppression file (5 minutes)
2. **Short-term:** Update `.cppcheck` configuration (5 minutes)
3. **Medium-term:** Add `compile_commands.json` generation to build (30 minutes)
4. **Long-term:** Consider CodeChecker for unified tool management

For the php-firebird project specifically, cppcheck remains a good choice when properly configured. The combination with the existing `.clang-tidy` configuration provides good coverage without adding new tool dependencies.

---

## References

- Cppcheck Manual: https://cppcheck.sourceforge.io/manual.pdf
- Cppcheck CTU Documentation: https://github.com/danmar/cppcheck/blob/main/doc/ctu.txt
- CodeChecker Documentation: https://codechecker.readthedocs.io/
- Clang Static Analyzer: https://clang-analyzer.llvm.org/
- compile_commands.json: https://clang.llvm.org/docs/JSONCompilationDatabase.html
- compiledb tool: https://github.com/nickdiego/compiledb

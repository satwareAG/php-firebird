# Static Analysis Integration Strategy (Phase 4 Preparation)

## Overview

Comprehensive static analysis integration for the modernized C++17 codebase. Establishes automated quality gates that validate memory safety, modern C++ adherence, and performance characteristics during Phase 3 implementation and beyond.

## Tool Integration Matrix

| Tool | Purpose | When to Run | Integration Level |
|------|---------|-------------|-------------------|
| **clang-tidy** | C++17 modernization checks, best practices | Pre-commit + CI/CD | Blocking |
| **Cppcheck** | Static analysis, undefined behavior detection | CI/CD | Warning + blocking on severe |
| **AddressSanitizer** | Memory error detection | Testing + CI/CD | Blocking on errors |
| **Valgrind** | Memory leak detection, profiling | Manual + nightly | Report only |
| **cppcheck-misra** | MISRA C++ compliance | Optional release validation | Advisory |

## 1. clang-tidy Configuration

### 1.1 .clang-tidy Configuration File

**File**: `.clang-tidy` (project root)

```yaml
# C++17 modernization and PHP extension specific checks
Checks: >
  modernize-*,
  performance-*,
  readability-*,
  bugprone-*,
  cppcoreguidelines-*,
  -modernize-use-trailing-return-type,
  -readability-braces-around-statements,
  -cppcoreguidelines-avoid-c-arrays,
  -cppcoreguidelines-pro-type-vararg

WarningsAsErrors: >
  modernize-use-nullptr,
  modernize-use-auto,
  modernize-use-override,
  performance-move-const-arg,
  bugprone-use-after-move

HeaderFilterRegex: 'firebird_utils\.(h|hpp|cpp)$'

CheckOptions:
  - key: modernize-use-auto.MinTypeNameLength
    value: 8
  - key: performance-for-range-copy.AllowedTypes
    value: 'ISC_STATUS;ISC_TIME;ISC_DATE'
  - key: readability-identifier-naming.ClassCase
    value: 'CamelCase'
  - key: readability-identifier-naming.FunctionCase  
    value: 'camelBack'
  - key: readability-identifier-naming.VariableCase
    value: 'camelBack'
  - key: readability-identifier-naming.ParameterCase
    value: 'camelBack'
```

### 1.2 PHP Extension Specific Rules

**Custom checks for extern "C" boundary safety:**

```yaml
# Additional PHP extension safety checks
ExtraArgs: 
  - '-DPHP_VERSION_8_1_PLUS'
  - '-DFB_API_VER=40'
  - '-I/usr/include/php/20210902'  # PHP 8.1 include path
  - '-I/usr/include/firebird'
  
# Custom check patterns
CustomChecks:
  - no-cpp-exceptions-in-extern-c: ERROR
  - no-std-containers-across-boundaries: WARNING  
  - validate-null-pointer-checks: ERROR
  - ensure-noexcept-for-c-interface: WARNING
```

### 1.3 clang-tidy Integration Script

**File**: `scripts/analysis/clang_tidy.sh`

```bash
#!/bin/bash
set -e

# clang-tidy validation script for C++17 modernization
echo "Running clang-tidy analysis..."

# Ensure compilation database exists
if ! [ -f compile_commands.json ]; then
    echo "Generating compilation database..."
    bear -- make clean && bear -- make
fi

# Run clang-tidy on C++ files only
clang-tidy firebird_utils.cpp \
    --config-file=.clang-tidy \
    --header-filter='firebird_utils\.(h|hpp)$' \
    --warnings-as-errors='modernize-use-nullptr,performance-*' \
    --format-style=file

# Check for blocking errors
if [ $? -ne 0 ]; then
    echo "❌ clang-tidy found blocking issues"
    exit 1
fi

echo "✅ clang-tidy analysis passed"
```

## 2. Cppcheck Integration

### 2.1 cppcheck Configuration

**File**: `.cppcheck` (project root)

```ini
# Cppcheck configuration for C++17 modernization
[cppcheck]
project=php-firebird.cppcheck

# Language standard
std=c++17

# Include paths
includePath=/usr/include/php/20210902
includePath=/usr/include/firebird
includePath=.

# Defines for conditional compilation
define=FB_API_VER=40
define=PHP_VERSION_8_1_PLUS

# Rules
enable=warning,performance,portability,information,missingInclude
disable=unusedFunction,missingIncludeSystem

# Severity levels
error-exitcode=2
inline-suppr=true

# File patterns
checks-max-time=300
check-config=true
```

### 2.2 Cppcheck CI Integration

**File**: `scripts/analysis/cppcheck.sh`

```bash
#!/bin/bash
set -e

echo "Running Cppcheck static analysis..."

# Create cppcheck project file if needed
if ! [ -f php-firebird.cppcheck ]; then
    cppcheck --project=compile_commands.json \
             --project-name=php-firebird \
             --std=c++17 \
             --enable=all \
             --inconclusive \
             --xml \
             --xml-version=2 \
             --output-file=cppcheck-report.xml \
             firebird_utils.cpp
else
    cppcheck --project=php-firebird.cppcheck \
             --xml \
             --xml-version=2 \
             --output-file=cppcheck-report.xml
fi

# Parse results
ERRORS=$(xmllint --xpath 'count(//error[@severity="error"])' cppcheck-report.xml 2>/dev/null || echo 0)
WARNINGS=$(xmllint --xpath 'count(//error[@severity="warning"])' cppcheck-report.xml 2>/dev/null || echo 0)

echo "Cppcheck results: $ERRORS errors, $WARNINGS warnings"

# Block on errors, allow warnings
if [ "$ERRORS" -gt 0 ]; then
    echo "❌ Cppcheck found $ERRORS errors"
    xmllint --xpath '//error[@severity="error"]' cppcheck-report.xml
    exit 1
fi

if [ "$WARNINGS" -gt 0 ]; then
    echo "⚠️ Cppcheck found $WARNINGS warnings (review recommended)"
fi

echo "✅ Cppcheck analysis completed"
```

## 3. Memory Safety Tools

### 3.1 AddressSanitizer Integration

**Compilation flags for development:**

```bash
# CMake configuration (if using CMake)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")

# Direct compilation (autotools)
CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
./configure --enable-shared \
    --with-php-config=/usr/bin/php-config8.1
```

**Testing script with AddressSanitizer:**

```bash
#!/bin/bash
# scripts/analysis/sanitizers.sh

set -e

echo "Building with AddressSanitizer..."

# Clean build with ASan
make clean
CXXFLAGS="-fsanitize=address -fno-omit-frame-pointer -g -O1" \
    make firebird_utils.lo

# Set ASan options
export ASAN_OPTIONS="abort_on_error=1:check_initialization_order=1:strict_init_order=1"
export ASAN_SYMBOLIZER_PATH="/usr/bin/llvm-symbolizer"

# Run PHP tests with ASan
echo "Running tests with AddressSanitizer..."
php -d extension=./modules/firebird.so -m | grep "firebird"

# Run specific test that exercises modernized functions
php -d extension=./modules/firebird.so tests/fbclient_vers_001.phpt

echo "✅ AddressSanitizer testing completed"
```

### 3.2 Valgrind Integration

Valgrind is essential for detecting memory leaks in PHP extensions. PHP's internal memory
management requires specific configuration for accurate results.

#### 3.2.1 Critical Environment Variables

| Variable | Value | Purpose |
|----------|-------|---------|
| `USE_ZEND_ALLOC` | `0` | Disables Zend's memory manager so Valgrind sees real malloc/free calls |
| `ZEND_DONT_UNLOAD_MODULES` | `1` | Keeps extension loaded for proper symbol resolution in stack traces |

**Why these matter:**
- PHP's Zend allocator pools memory, hiding individual allocations from Valgrind
- Without `ZEND_DONT_UNLOAD_MODULES=1`, stack traces show `???` instead of function names
- Both are **mandatory** for meaningful PHP extension memory analysis

#### 3.2.2 Recommended Valgrind Options

```bash
VALGRIND_OPTS="
    --tool=memcheck
    --leak-check=full
    --show-leak-kinds=definite,indirect,possible
    --track-origins=yes
    --num-callers=30
    --error-exitcode=1
    --errors-for-leak-kinds=definite,indirect
    --suppressions=valgrind-php.supp"
```

| Option | Purpose |
|--------|---------|
| `--track-origins=yes` | Shows source of uninitialized values (slower but essential for debugging) |
| `--num-callers=30` | Deep stack traces for complex PHP extension call chains |
| `--errors-for-leak-kinds=definite,indirect` | Fail only on real leaks, not reachable memory |
| `--suppressions=valgrind-php.supp` | Filter known false positives |

#### 3.2.3 Leak Classification

| Leak Type | Severity | Action |
|-----------|----------|--------|
| **definite** | Critical | Must fix - memory definitely lost |
| **indirect** | Critical | Must fix - memory lost via other lost blocks |
| **possible** | Warning | Review - may be false positive or real leak |
| **reachable** | Info | Usually false positive - suppress in production |

Focus on **definite** and **indirect** leaks. Reachable memory in PHP/Firebird internals
is typically intentional caching and should be suppressed.

#### 3.2.4 Using the Valgrind Script

The project provides `scripts/analysis/valgrind.sh` with three test modes:

```bash
# Quick test - extension load only (fastest)
./scripts/analysis/valgrind.sh --quick

# Full test - extension load + function coverage + connection test
./scripts/analysis/valgrind.sh --full

# Tests mode - run PHPT test suite under Valgrind (slowest, most thorough)
./scripts/analysis/valgrind.sh --tests
```

**Script features:**
- Automatic extension and suppression file detection
- Docker-compatible path resolution
- Color-coded output with pass/fail status
- Multiple test modes for different scenarios

#### 3.2.5 Suppression File Maintenance

The `valgrind-php.supp` file filters known false positives from PHP and Firebird internals.

**When to update suppressions:**
- After PHP version upgrades (new internal allocations)
- After Firebird client library updates
- When valid reachable memory is flagged

**Structure of suppression entries:**

```ini
# valgrind-php.supp - Suppress known PHP/Firebird internal allocations
#
# NOTE: Only suppress 'reachable' leaks (false positives from internals).
# Never suppress 'definite' or 'indirect' leaks.

{
   php_module_startup_allocations
   Memcheck:Leak
   match-leak-kinds: reachable
   ...
   fun:php_module_startup*
}

{
   firebird_client_internal_caching
   Memcheck:Leak
   match-leak-kinds: reachable
   obj:*libfbclient.so*
}

{
   zend_string_interning
   Memcheck:Leak
   match-leak-kinds: reachable
   ...
   fun:zend_interned_string*
}
```

#### 3.2.6 Generating New Baseline Suppressions

After a PHP or Firebird version upgrade, generate a new baseline:

```bash
# 1. Set required environment variables
export USE_ZEND_ALLOC=0
export ZEND_DONT_UNLOAD_MODULES=1

# 2. Generate suppressions for all reachable memory
valgrind --leak-check=full \
         --show-leak-kinds=reachable \
         --gen-suppressions=all \
         php -d extension=./modules/firebird.so \
         -r "echo 'Baseline';" \
         2>&1 | tee valgrind-baseline.log

# 3. Extract suppression blocks from log
grep -A 20 "^{" valgrind-baseline.log > new-suppressions.txt

# 4. Review and merge relevant suppressions into valgrind-php.supp
# Only add suppressions for reachable memory from PHP/Firebird internals
```

**Workflow for adding new suppressions:**

1. Run Valgrind without the new suppression
2. Identify the leak source (PHP internal? Firebird client? Extension code?)
3. If PHP/Firebird internal: add suppression with descriptive name
4. If extension code: fix the leak, don't suppress
5. Commit updated suppression file with explanatory comment

#### 3.2.7 CI/CD Integration

```yaml
# GitLab CI example
valgrind:
  stage: memory-safety
  image: ubuntu:22.04
  variables:
    # CRITICAL: Required for accurate PHP extension memory analysis
    USE_ZEND_ALLOC: '0'
    ZEND_DONT_UNLOAD_MODULES: '1'
  before_script:
    - apt-get update -qq
    - apt-get install -y valgrind php8.3-dev libfirebird-dev
  script:
    - ./scripts/analysis/valgrind.sh --full
  artifacts:
    paths:
      - valgrind-*.log
    when: always
  allow_failure: true  # Advisory only for nightly builds
  rules:
    - if: $CI_PIPELINE_SOURCE == "schedule"  # Nightly only
```

#### 3.2.8 Troubleshooting

**Problem: Stack traces show `???` instead of function names**
```
Solution: Ensure ZEND_DONT_UNLOAD_MODULES=1 is set before running Valgrind
```

**Problem: Thousands of "reachable" leaks from PHP internals**
```
Solution: Use --suppressions=valgrind-php.supp and focus on definite/indirect leaks
```

**Problem: Valgrind runs extremely slowly**
```
Solution: Use --quick mode for development, --full/--tests for CI/release validation
```

**Problem: False positives from Firebird client library**
```
Solution: Add suppression entry matching obj:*libfbclient.so* for reachable memory
```

## 4. CI/CD Pipeline Integration

### 4.1 GitLab CI Configuration

**File**: `.gitlab-ci.yml` (add to existing pipeline)

```yaml
stages:
  - validate
  - build
  - test
  - static-analysis
  - memory-safety

variables:
  CPPCHECK_VERSION: "2.12"
  CLANG_TIDY_VERSION: "17"

# Static analysis stage
clang-tidy:
  stage: static-analysis
  image: ubuntu:22.04
  before_script:
    - apt-get update -qq
    - apt-get install -y clang-tools-17 bear make gcc php8.1-dev libfirebird-dev
    - ln -sf /usr/bin/clang-tidy-17 /usr/bin/clang-tidy
  script:
    - ./scripts/analysis/clang_tidy.sh
  artifacts:
    reports:
      codequality: clang-tidy-report.json
    paths:
      - compile_commands.json
      - clang-tidy-report.json
  allow_failure: false

cppcheck:
  stage: static-analysis
  image: ubuntu:22.04
  before_script:
    - apt-get update -qq
    - apt-get install -y cppcheck xmllint
  script:
    - ./scripts/analysis/cppcheck.sh
  artifacts:
    reports:
      codequality: cppcheck-report.xml
    paths:
      - cppcheck-report.xml
  allow_failure: false

# Memory safety testing
address-sanitizer:
  stage: memory-safety
  image: ubuntu:22.04
  before_script:
    - apt-get update -qq
    - apt-get install -y gcc clang php8.1-dev libfirebird-dev
  script:
    - ./scripts/analysis/sanitizers.sh
  artifacts:
    paths:
      - asan-report.log
  allow_failure: false
  
valgrind:
  stage: memory-safety
  image: ubuntu:22.04
  variables:
    # CRITICAL: Required for accurate PHP extension memory analysis
    USE_ZEND_ALLOC: '0'              # Disable Zend allocator for precise tracking
    ZEND_DONT_UNLOAD_MODULES: '1'    # Keep modules loaded for stack traces
  before_script:
    - apt-get update -qq
    - apt-get install -y valgrind php8.1-dev libfirebird-dev
  script:
    - ./scripts/analysis/valgrind.sh --full
  artifacts:
    paths:
      - valgrind-*.log
    when: always
  allow_failure: true  # Advisory only
  rules:
    - if: $CI_PIPELINE_SOURCE == "schedule"  # Nightly only
```

### 4.2 Pre-commit Hook Integration

**File**: `.git/hooks/pre-commit` (or use husky if Node.js available)

```bash
#!/bin/bash
# Pre-commit hook for C++17 modernization validation

set -e

echo "Running pre-commit C++ analysis..."

# Check if C++ files are staged
CPP_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep '\.(cpp|hpp|h)$' || true)

if [ -z "$CPP_FILES" ]; then
    echo "No C++ files staged, skipping analysis"
    exit 0
fi

# Quick clang-tidy check on staged files only
echo "Running clang-tidy on staged files..."
for file in $CPP_FILES; do
    if [ -f "$file" ]; then
        clang-tidy "$file" \
            --checks="-*,modernize-use-nullptr,performance-move-const-arg,bugprone-use-after-move" \
            --warnings-as-errors="modernize-use-nullptr,performance-move-const-arg" \
            --quiet
        
        if [ $? -ne 0 ]; then
            echo "❌ clang-tidy found issues in $file"
            echo "Run: clang-tidy $file --fix-errors"
            exit 1
        fi
    fi
done

echo "✅ Pre-commit C++ analysis passed"
```

## 5. IDE Integration (CLion)

### 5.1 CLion Configuration

**File**: `.idea/codeStyleSettings.xml`

```xml
<component name="ProjectCodeStyleConfiguration">
  <state>
    <option name="PREFERRED_PROJECT_CODE_STYLE" value="C++17_Modern" />
  </state>
</component>
```

**File**: `.idea/inspectionProfiles/cpp17_profile.xml`

```xml
<component name="InspectionProjectProfileManager">
  <profile version="1.0">
    <option name="myName" value="C++17 Modernization" />
    <inspection_tool class="modernizeLoopConvert" enabled="true" level="WARNING" />
    <inspection_tool class="modernizeUseNullptr" enabled="true" level="ERROR" />
    <inspection_tool class="modernizeUseOverride" enabled="true" level="WARNING" />
    <inspection_tool class="modernizeUseAuto" enabled="true" level="WEAK_WARNING" />
    <inspection_tool class="cppcoreguidelines-pro-type-reinterpret-cast" enabled="true" level="WARNING" />
  </profile>
</component>
```

### 5.2 External Tools Configuration

**CLion External Tools** (Settings → Tools → External Tools):

```xml
<toolSet name="Static Analysis">
  <tool name="clang-tidy-fix" 
        program="clang-tidy" 
        arguments="$FilePath$ --fix --format-style=file"
        workingDirectory="$ProjectFileDir$" />
        
  <tool name="cppcheck-file"
        program="cppcheck"
        arguments="--enable=all --std=c++17 --inconclusive $FilePath$"
        workingDirectory="$ProjectFileDir$" />
        
  <tool name="format-code"
        program="clang-format"
        arguments="-i --style=file $FilePath$"
        workingDirectory="$ProjectFileDir$" />
</toolSet>
```

## 6. Code Quality Gates

### 6.1 Quality Thresholds

```yaml
# Quality gates configuration
quality_thresholds:
  clang_tidy:
    max_errors: 0           # Zero tolerance for errors
    max_warnings: 5         # Allow minor warnings
    
  cppcheck:
    max_errors: 0           # Zero tolerance for errors  
    max_warnings: 10        # Advisory warnings allowed
    
  address_sanitizer:
    max_errors: 0           # Zero memory errors allowed
    
  valgrind:
    max_definite_leaks: 0   # No definite leaks
    max_possible_leaks: 5   # Some possible leaks acceptable
```

### 6.2 Automated Quality Reports

**File**: `scripts/generate_quality_report.sh`

```bash
#!/bin/bash
# Generate comprehensive quality report

set -e

REPORT_DIR="reports/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$REPORT_DIR"

echo "Generating comprehensive quality report in $REPORT_DIR..."

# 1. clang-tidy report
echo "Running clang-tidy analysis..."
clang-tidy firebird_utils.cpp \
    --config-file=.clang-tidy \
    --export-fixes="$REPORT_DIR/clang-tidy-fixes.yaml" \
    > "$REPORT_DIR/clang-tidy-report.txt"

# 2. Cppcheck report  
echo "Running Cppcheck analysis..."
cppcheck firebird_utils.cpp \
    --xml --xml-version=2 \
    --output-file="$REPORT_DIR/cppcheck-report.xml"

# 3. Code metrics
echo "Collecting code metrics..."
cloc firebird_utils.cpp firebird_utils.h > "$REPORT_DIR/code-metrics.txt"

# 4. Compilation test with different optimization levels
echo "Testing compilation with various optimization levels..."
for OPT in O0 O1 O2 O3; do
    echo "Testing -$OPT optimization..."
    g++ -std=c++17 -$OPT -c firebird_utils.cpp -o /tmp/test_$OPT.o 2>&1 | tee "$REPORT_DIR/compile-$OPT.log"
done

# 5. Generate summary report
cat > "$REPORT_DIR/summary.md" << EOF
# Quality Report - $(date)

## clang-tidy Results
$(grep -c "warning:" "$REPORT_DIR/clang-tidy-report.txt" || echo 0) warnings found
$(grep -c "error:" "$REPORT_DIR/clang-tidy-report.txt" || echo 0) errors found

## Cppcheck Results  
$(xmllint --xpath 'count(//error)' "$REPORT_DIR/cppcheck-report.xml" 2>/dev/null || echo 0) issues found

## Code Metrics
$(cat "$REPORT_DIR/code-metrics.txt")

## Recommendations
Based on the analysis, focus modernization efforts on:
$(head -5 "$REPORT_DIR/clang-tidy-report.txt" | grep "warning:" | cut -d: -f4- | sort | uniq)
EOF

echo "✅ Quality report generated: $REPORT_DIR/summary.md"
```

## 7. Performance Validation

### 7.1 Benchmarking Framework

**File**: `tests/benchmarks/cpp17_performance_test.cpp`

```cpp
#include <chrono>
#include <vector>
#include <iostream>
#include "../firebird_utils.h"

class PerformanceBenchmark {
private:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = std::chrono::nanoseconds;
    
    template<typename Func>
    Duration measure_execution_time(Func&& func, int iterations = 10000) {
        auto start = Clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        
        auto end = Clock::now();
        return std::chrono::duration_cast<Duration>(end - start);
    }
    
public:
    void benchmark_client_version() {
        void* mock_master = setup_mock_master();
        
        auto duration = measure_execution_time([&]() {
            volatile unsigned version = fbu_get_client_version(mock_master);
            (void)version; // Prevent optimization
        });
        
        std::cout << "fbu_get_client_version: " 
                  << duration.count() / 10000.0 << " ns/call\n";
    }
    
    void benchmark_encode_functions() {
        void* mock_master = setup_mock_master();
        
        auto time_duration = measure_execution_time([&]() {
            volatile ISC_TIME time = fbu_encode_time(mock_master, 10, 30, 45, 123);
            (void)time;
        });
        
        auto date_duration = measure_execution_time([&]() {
            volatile ISC_DATE date = fbu_encode_date(mock_master, 2025, 11, 19);
            (void)date;
        });
        
        std::cout << "fbu_encode_time: " << time_duration.count() / 10000.0 << " ns/call\n";
        std::cout << "fbu_encode_date: " << date_duration.count() / 10000.0 << " ns/call\n";
    }
};
```

### 7.2 Performance Regression Detection

**File**: `scripts/performance_validation.sh`

```bash
#!/bin/bash
# Performance regression detection

set -e

echo "Running performance validation..."

# Baseline measurements (before modernization)
BASELINE_DIR="benchmarks/baseline"
CURRENT_DIR="benchmarks/current"

mkdir -p "$BASELINE_DIR" "$CURRENT_DIR"

# Run benchmarks
./tests/benchmarks/cpp17_performance_test > "$CURRENT_DIR/results.txt"

# Compare with baseline if available
if [ -f "$BASELINE_DIR/results.txt" ]; then
    echo "Comparing with baseline performance..."
    
    # Extract timing data and calculate percentage difference
    python3 scripts/compare_performance.py \
        "$BASELINE_DIR/results.txt" \
        "$CURRENT_DIR/results.txt"
else
    echo "No baseline found, establishing new baseline..."
    cp "$CURRENT_DIR/results.txt" "$BASELINE_DIR/results.txt"
fi

echo "✅ Performance validation completed"
```

## 8. Quality Dashboard

### 8.1 Automated Quality Metrics Collection

**File**: `scripts/collect_quality_metrics.py`

```python
#!/usr/bin/env python3
import json
import xml.etree.ElementTree as ET
import re
from pathlib import Path

def collect_clang_tidy_metrics(report_path):
    """Parse clang-tidy report and extract metrics."""
    with open(report_path) as f:
        content = f.read()
    
    warnings = len(re.findall(r'warning:', content))
    errors = len(re.findall(r'error:', content))
    modernize_issues = len(re.findall(r'modernize-', content))
    
    return {
        'warnings': warnings,
        'errors': errors,
        'modernize_issues': modernize_issues
    }

def collect_cppcheck_metrics(report_path):
    """Parse cppcheck XML report."""
    tree = ET.parse(report_path)
    root = tree.getroot()
    
    issues_by_severity = {}
    for error in root.findall('.//error'):
        severity = error.get('severity', 'unknown')
        issues_by_severity[severity] = issues_by_severity.get(severity, 0) + 1
    
    return issues_by_severity

def generate_quality_report():
    """Generate consolidated quality metrics."""
    metrics = {
        'timestamp': '2025-11-19T15:00:00',
        'clang_tidy': collect_clang_tidy_metrics('reports/clang-tidy-report.txt'),
        'cppcheck': collect_cppcheck_metrics('reports/cppcheck-report.xml'),
        'modernization_progress': calculate_modernization_progress()
    }
    
    with open('reports/quality-metrics.json', 'w') as f:
        json.dump(metrics, f, indent=2)
    
    print("Quality metrics collected successfully")

if __name__ == '__main__':
    generate_quality_report()
```

## 9. Documentation Integration

### 9.1 Quality Gate Documentation

**File**: `docs/development/QUALITY_GATES.md`

```markdown
# C++17 Modernization Quality Gates

## Automated Checks (CI/CD)

### Blocking Gates:
- [ ] clang-tidy: Zero errors, <5 warnings
- [ ] Cppcheck: Zero errors  
- [ ] AddressSanitizer: Zero memory errors
- [ ] Compilation: Success on all supported compilers

### Advisory Gates:
- [ ] Valgrind: Report memory usage patterns
- [ ] Performance: <1% regression from baseline

## Manual Review Checklist:

### Code Quality:
- [ ] Modern C++17 patterns used consistently
- [ ] RAII implemented for all resource management
- [ ] Exception safety guarantees documented
- [ ] const/noexcept specifications applied

### Compatibility:
- [ ] extern "C" interfaces preserved exactly
- [ ] No C++ exceptions cross C boundaries
- [ ] Thread safety characteristics maintained
- [ ] Binary compatibility validated

### Testing:
- [ ] Unit tests for all modernized functions
- [ ] Integration tests pass without modification
- [ ] Performance benchmarks show no regression
- [ ] Cross-platform compilation verified
```

## 10. Integration with Build System

### 10.1 Autotools Integration

**File**: `config.m4` (add static analysis support)

```m4
# Add static analysis support
AC_ARG_ENABLE([static-analysis],
  [AS_HELP_STRING([--enable-static-analysis], 
    [Enable static analysis tools integration (clang-tidy, cppcheck)])],
  [enable_static_analysis=$enableval],
  [enable_static_analysis=no])

if test "$enable_static_analysis" = "yes"; then
  AC_PATH_PROG([CLANG_TIDY], [clang-tidy])
  AC_PATH_PROG([CPPCHECK], [cppcheck])
  
  if test -z "$CLANG_TIDY"; then
    AC_MSG_ERROR([clang-tidy not found but static analysis requested])
  fi
  
  if test -z "$CPPCHECK"; then
    AC_MSG_WARN([cppcheck not found, some static analysis features disabled])
  fi
  
  AC_DEFINE([ENABLE_STATIC_ANALYSIS], [1], [Static analysis tools enabled])
fi
```

### 10.2 Makefile Integration

**File**: `Makefile.frag` (if using PHP extension build system)

```makefile
# Static analysis targets
.PHONY: static-analysis clang-tidy cppcheck asan-test valgrind-test

static-analysis: clang-tidy cppcheck

clang-tidy:
	@echo "Running clang-tidy analysis..."
	./scripts/analysis/clang_tidy.sh

cppcheck:
	@echo "Running cppcheck analysis..."
	./scripts/analysis/cppcheck.sh

asan-test:
	@echo "Running AddressSanitizer tests..."
	./scripts/analysis/sanitizers.sh

valgrind-test:
	@echo "Running Valgrind analysis..."
	./scripts/analysis/valgrind.sh --quick

# Quality gate - runs all checks
quality-gate: static-analysis asan-test
	@echo "✅ All quality gates passed"
```

## Success Criteria

### Tool Configuration Complete When:
- [ ] All static analysis tools configured and integrated
- [ ] CI/CD pipeline includes quality gates
- [ ] Pre-commit hooks validate basic quality
- [ ] IDE integration provides real-time feedback
- [ ] Performance benchmarking framework operational

### Quality Standards Met When:
- [ ] clang-tidy: Zero errors on modernized code
- [ ] Cppcheck: Zero errors, minimal warnings
- [ ] AddressSanitizer: Clean test execution
- [ ] Valgrind: No definite memory leaks
- [ ] Performance: No regression from baseline

This static analysis integration ensures the C++17 modernization maintains high quality standards while providing continuous feedback during development.

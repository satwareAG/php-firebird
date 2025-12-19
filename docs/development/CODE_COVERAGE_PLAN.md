# Code Coverage Strategy (2025)

**Status:** Baseline Established
**Current Coverage:** 70.9% (Line) / 91.5% (Function)
**Date:** 2025-11-28

## 1. Strategic Goals

In line with 2025 Best Practices for critical infrastructure (database drivers) and legacy modernization:

| Metric | Current | Target (Phase 1) | Target (Phase 2) | Industry Std |
|--------|---------|------------------|------------------|--------------|
| **Line Coverage** | 70.9% | **65%** | **75%** | ≥80% |
| **Function Coverage** | 91.5% | **85%** | **90%** | ≥90% |
| **New Code** | N/A | **≥80%** | **≥90%** | ≥90% |

### Rationale
- **Current Baseline (60.7%)**: Solid for a C extension, but lower than modern managed code standards due to error handling branches and legacy components (`ibase_events.c`).
- **Modernization Target (75%)**: The modernized `firebird_utils.cpp` already achieves **75.6%**. This proves 75% is realistic for the core codebase.
- **Infrastructure Gap**: `ibase_events.c` (13%) and `ibase_service.c` (38%) drag the average down due to test environment limitations (threading models, service API access).

## 2. Current Baseline Analysis

| File | Lines | Hit | Coverage | Status | Notes |
|------|-------|-----|----------|--------|-------|
| `firebird_utils.cpp` | 217 | 164 | **75.6%** | 🟢 Good | Modern C++17 implementation |
| `ibase_blobs.c` | 233 | 172 | **73.8%** | 🟢 Good | Blob coverage verified |
| `ibase_query.c` | 1727 | 1196 | **69.3%** | 🟡 Decent | Core query logic |
| `ibase_service.c` | 309 | 233 | **75.4%** | 🟢 Good | Service API (User/Maint/Info) |
| `ibase_events.c` | 180 | 24 | **13.3%** | 🔴 Critical | Recursion crash on PHP 8.3 (Disabled) |

## 3. Improvement Plan

### Phase 1: Quality Gates (Immediate)
- **Enforce No Regression**: CI should fail if coverage drops below **60%**.
- **New Code Rule**: Any *new* C++/C file must hit **80%** coverage.

### Phase 2: Targeting the Low Hanging Fruit (Short-term)
1.  **Blob Coverage** (Completed):
    - Added `tests/ibase_blob_001.phpt` (Handle validation/Cancellation).
    - Added `tests/ibase_blob_002.phpt` (Chunked reads).
    - Added `tests/ibase_blob_003.phpt` (Large >64KB segmentation).
    - **Added `tests/ibase_blob_coverage.phpt`**: Covers `ibase_blob_info`, `ibase_blob_echo`, `ibase_blob_import`.
    - Result: 49.3% → 73.8% line coverage.
2.  **Service API** (Completed):
    - Enabled basic Service Manager tests (`ibase_service_001.phpt`, `002.phpt`).
    - **Added `tests/ibase_service_user.phpt`**: Full CRUD for user management.
    - **Added `tests/ibase_service_db_mgr.phpt`**: Database info and maintenance.
    - Result: 39.2% → 75.4% line coverage.

### Phase 3: Architecture Fixes (Incomplete/Blocked)
- **Event Handling**: Attempted to enable `tests/008.phpt`.
- **Result**: Passed on PHP 8.1 but caused **Stack Overflow (Infinite Recursion)** on PHP 8.3.
- **Action**: Test re-disabled to ensure CI stability. Requires deep re-architecting of callback mechanism (remove `isc_que_events` from callback path).

## 4. Security & Confidentiality Implications

Increasing code coverage must be balanced with security and confidentiality requirements:

- **Secret Containment:** Tests covering authentication flows (`ibase_service.c`, `ibase_connect`) must rely on environment variables and temporary credentials. Coverage reports should exclude test data files to prevent accidental secret leakage in artifacts.
- **Fuzzing Integration:** Higher coverage reveals more code paths. We must pair coverage increases with **fuzzing** (using OSS-Fuzz or similar) to ensure these newly explored paths are robust against malicious input, especially in `ibase_query.c`.
- **Legacy Risks:** Components like `ibase_events.c` are being targeted for coverage. As we expose this legacy code to more tests, we may uncover latent security vulnerabilities (buffer overflows, race conditions). A **security audit** of newly covered legacy regions is mandatory before counting them towards the "stable" baseline.

## 5. Rollback & Failure Strategy

If critical components drop below the baseline during CI/CD:

1.  **Blocking Strategy:** The pipeline **fails immediately** (Exit Code 1).
2.  **Rollback Trigger:** If a merge request caused the drop, it is rejected. If a commit landed solely updating tests but revealing lower coverage, we revert the commit.
3.  **Emergency Override:** In case of critical security hotfixes where full coverage cannot be guaranteed immediately, a **manual approval gate** protected by `CODEOWNERS` (Project Lead) allows bypassing the check. This event must be logged as a technical debt item.

## 6. CI/CD Integration

We recommend adding a configurable coverage gate to `.gitlab-ci.yml`. The threshold is defined as a variable to allow updates without modifying the script logic.

```yaml
variables:
  COVERAGE_THRESHOLD: "60.0"

coverage:check:
  stage: validate
  script:
    - docker-compose run --rm php81-dev /ext/docker/scripts/coverage.sh
    - |
      COVERAGE=$(grep -oP 'lines......: \K\d+\.\d+' coverage/lcov_filtered.info | head -1)
      echo "Coverage: $COVERAGE%"
      if (( $(echo "$COVERAGE < $COVERAGE_THRESHOLD" | bc -l) )); then
        echo "❌ Coverage dropped below baseline ($COVERAGE_THRESHOLD%)"
        exit 1
      fi

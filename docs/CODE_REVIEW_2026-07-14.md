# Code Review: feat/v13-decfloat-native-type

**Branch**: `feat/v13-decfloat-native-type` (17 commits ahead of `satware-main`)
**Stats**: 91 files changed, +846 / -6,899 lines
**CI**: All 5 workflows green (12/12 PHP/FB jobs pass, 74-128s each)

---

## Commit Groups

### Group 1: Ponytail Audit (7 commits, -6,500 lines)

Dead code, dead scripts, dead docs, dead specs removal.

| Commit | Description | Files | LOC |
|--------|-------------|-------|-----|
| `2b12909` | PDO DECFLOAT/INT128 bind fix + 3 dead C++ headers | 5 | -1,745 |
| `6f01d03` | Delete 8 dead scripts | 10 | -3,302 |
| `31dfa15` | Archive 9 docs + 17 specs | 26 | (move only) |
| `c39ba97` | Dead C functions, FBIRD_ARRAY_DEBUG, dead class members | 8 | -777 |
| `67f1f17` | Status-copy consolidation, `fbm_*` template, `set_status_error()` | 5 | -640 |
| `5c8cb5a` | Stale PHP 8.1 refs, branch-alias, dead links | 6 | -20 |
| `df18829` | Delete 5 dead test helper functions | 3 | -15 |

**Review focus**: Verify no live code was deleted. All deletions are dead code
confirmed by `rg` search with zero callers.

### Group 2: DECFLOAT Bug Fixes (8 commits)

Root cause analysis and fixes for the 30-minute CI hang.

| Commit | Description | Key File |
|--------|-------------|----------|
| `9564fc1` | PDO DEC34 type masking + IStatus leaks (25 functions) | `pdo_fbird_stmt.c`, `firebird_utils.cpp`, `fb_statement.hpp` |
| `0454523` | FB4 client in Dockerfiles + PDO `length` detection + `--set-timeout` | `Dockerfile-8.{2,3,4,5}`, `ci.yml`, `test.sh` |
| `3b697c1` | `pdo_fbird` version reads `VERSION.txt` | `pdo_fbird/config.m4`, `php_pdo_fbird.h` |
| `d4b15a0` | **Root cause fix**: `FIREBIRD_DB_DIR=/tmp` in CI | `ci.yml` |
| `ca4bfo2` | 3 CI blockers: test fix + struct init + branch filter | `fb3_wire_protocol.phpt`, `pdo_fbird_driver.c`, `code-quality.yml` |
| `a43cee6` | CheckStatusScope in ServiceWrapper (attempt) | `fb_service.hpp` |
| `4b72480` | Missing `fb_status.hpp` include (attempt) | `fb_service.hpp` |
| `9d670f7` | **Revert**: ServiceWrapper to original (Firebird holds IStatus ref) | `fb_service.hpp` |

**Review focus**:
- `pdo_fbird_stmt.c:642-663`: PDO DECFLOAT detection via `length >= 16`
- `ci.yml:141-148`: `FIREBIRD_DB_DIR: /tmp` (the critical fix)
- `Dockerfile-8.{2,3,4,5}`: Official FB4 client tarball (not apt `firebird-dev`)
- `fb_service.hpp`: Reverted to original (no IStatus dispose)
- `pdo_fbird_driver.c:1223`: Removed excess NULL (16 fields, not 17)

### Group 3: Process/Docs (2 commits)

| Commit | Description |
|--------|-------------|
| `08d3691` | AGENTS.md parity rules, CONTRIBUTING.md IStatus rules, `verify-ci-parity.sh` |

---

## Key Files for Review

### `pdo_fbird/pdo_fbird_stmt.c` (+15/-12)
- **Line 642-663**: DECFLOAT case uses `length >= 16` instead of `sql_type == SQL_DEC34`
- Changed from `if (sql_type == SQL_DEC34)` to `if (length >= 16)` for robust DEC16/DEC34 distinction

### `.github/workflows/ci.yml` (+5/-0)
- **Line 145**: `FIREBIRD_DB_DIR: /tmp` (root cause fix for metadata lock hang)
- **Line 246**: `--set-timeout 15` (fail fast instead of 60s default)

### `docker/php/Dockerfile-8.{2,3,4,5}` (+136/-12 each)
- Replaced `firebird-dev` apt package with official FB4 client tarball download
- Added `pdo` to `docker-php-ext-install` (was missing in base images)
- Added `FIREBIRD_HOME`, `LD_LIBRARY_PATH` env vars

### `firebird_utils.cpp` (+210/-762)
- Consolidated 8 status-copy loops into `copy_status_to_sv()` helper
- Added `fbm_*` template pattern for metadata accessor functions
- Removed 865-line `firebird_utils_typed.h` (dead code)
- Added IStatus `dispose()` in DECFLOAT/INT128 conversion functions

### `src/cpp/fb_statement.hpp` (+35/-88)
- Added IStatus `dispose()` before every return in 13 StatementWrapper methods
- Consolidated duplicate status-check code

### `src/cpp/fb_status.hpp` (+30/-140)
- Added `CheckStatusScope` RAII wrapper for IStatus
- Added `set_status_error()` and `copy_status_to_sv()` helpers

---

## Regression Risk Assessment

| Change | Risk | Mitigation |
|--------|------|------------|
| Dead code deletion | Low | Zero callers found via `rg` |
| PDO DECFLOAT `length` detection | Low | Passes on all 12 PHP/FB combos |
| `FIREBIRD_DB_DIR=/tmp` in CI | None | Fixes lock conflict, no side effects |
| Dockerfile FB4 client | Low | Matches CI exactly, full test matrix passes |
| `fb_service.hpp` revert | None | Restored to pre-change state |
| IStatus `dispose()` in utils | Medium | Works for utility functions; unsafe for service wrappers |

---

## Test Results

### CI (commit 9d670f7, run 29329523959)

| PHP | FB 3.0 | FB 4.0 | FB 5.0 |
|-----|--------|--------|--------|
| 8.2 | PASS (127s) | PASS (84s) | PASS (87s) |
| 8.3 | PASS (80s) | PASS (84s) | PASS (85s) |
| 8.4 | PASS (76s) | PASS (120s) | PASS (89s) |
| 8.5 | PASS (74s) | PASS (87s) | PASS (86s) |

All 5 workflows green: CI, Code Quality, Sanitizers, Coverage, doctrine-downstream.

### Known Flaky Tests

- `client_coverage/client_iservice.phpt`: Intermittent Termsig=11 on CI (service manager segfault). Passes on rerun. Pre-existing.
- `fbird_name_result_001.phpt`: Row ordering without ORDER BY (Issue #470).

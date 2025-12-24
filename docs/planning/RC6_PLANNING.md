# v7.0.0-rc.6 Release Planning

**Created:** 2025-12-24
**Target Release:** v7.0.0-rc.6
**Previous Release:** v7.0.0-rc.5 (2025-12-24)
**Branch:** feature/fbird-extension-release

---

## Executive Summary

Release rc.6 includes two significant features from the Unreleased changelog:
1. **Exception Mode API (#15)** - PDO-style exception handling for Doctrine DBAL compatibility
2. **Fork-safety (#22)** - Segfault prevention for pcntl_fork, PHPStan parallel, PHPUnit parallel

Both features are **fully implemented and tested**. This document outlines verification tasks and release criteria.

---

## Features

### 1. Exception Mode API (#15)

**Purpose:** Enable PDO-style `ERRMODE_EXCEPTION` behavior required for Doctrine DBAL integration.

#### API Surface

| Function/Constant | Description |
|-------------------|-------------|
| `fbird_set_exception_mode(int $mode): bool` | Set runtime exception mode |
| `fbird_get_exception_mode(): int` | Get current exception mode |
| `FBIRD_EXCEPTION_MODE_SILENT` (0) | Default: return false on error, no exception |
| `FBIRD_EXCEPTION_MODE_THROW` (1) | Throw `Firebird\Exception` on error |
| `Firebird\Exception::getSqlState(): string` | Return 5-char SQLSTATE code (e.g., "23000", "42000") |

#### Behavior

- **SILENT mode (default):** Functions return `false` on error, error message available via `fbird_errmsg()`
- **THROW mode:** Functions throw `Firebird\Exception` with code, message, and SQLSTATE
- **Precedence:** Runtime mode via `fbird_set_exception_mode()` overrides INI `fbird.enable_exceptions`
- **Backward compatible:** Default SILENT behavior maintains existing code compatibility

#### Test Coverage

| Test File | Coverage |
|-----------|----------|
| `tests/fbird_exception_mode_001.phpt` | Constants, functions, mode switching, invalid mode |
| `tests/fbird_exception_mode_002.phpt` | Database errors in SILENT/THROW modes, exception properties |

#### Usage Example

```php
// Enable exception mode (PDO-style)
fbird_set_exception_mode(FBIRD_EXCEPTION_MODE_THROW);

try {
    $result = fbird_query($conn, 'SELECT * FROM non_existent_table');
} catch (Firebird\Exception $e) {
    echo "Error: " . $e->getMessage() . "\n";
    echo "SQLSTATE: " . $e->getSqlState() . "\n"; // e.g., "42S02"
    echo "Code: " . $e->getCode() . "\n";
}
```

---

### 2. Fork-safety (#22)

**Purpose:** Prevent segmentation faults when PHP forks processes while the Firebird extension is loaded.

#### Problem Addressed

When `pcntl_fork()` is called, child processes inherit parent's resource handles. On exit, child destructors attempted Firebird API cleanup on parent's handles, causing segfaults.

**Affected tools:**
- `pcntl_fork()` direct usage
- PHPStan parallel mode
- PHPUnit parallel mode
- Infection mutation testing
- Psalm parallel mode
- Custom worker pools

#### Solution

- **PID tracking:** Store `init_pid` during GINIT
- **Fork detection:** Destructors compare `getpid()` with `init_pid`
- **Conditional cleanup:** Skip Firebird API cleanup in forked children
- **Parent-only cleanup:** Only original process performs connection/transaction cleanup

#### Test Coverage

| Test File | Coverage |
|-----------|----------|
| `tests/issue22_pcntl_fork_001.phpt` | Fork → child exit code 0 (not 139 segfault) |

#### Verification

```bash
# Test passes when child exits with code 0 (not 139)
run-tests.php tests/issue22_pcntl_fork_001.phpt
```

---

## Doctrine Integration Analysis

### Issue #33 (TIME Tests) - NO ACTION REQUIRED

**Status:** Fixed in php-firebird v7.0.0-rc.3
**Root cause:** Uninitialized struct fields in `fbird_parse_time()` caused TIME corruption
**Fix:** Commit 0f469fe (December 22, 2025)

**Action:** This is a **doctrine-firebird-driver** issue. They need to enable previously skipped TIME tests. No php-firebird changes needed.

### Issue #23 (Transaction Deadlock) - NO EXTENSION BUG

**Analysis:** The doctrine driver was using `SET TRANSACTION` SQL via `fbird_query()`, which triggers implicit transaction creation before executing the SQL statement.

**Root cause (driver architecture):**
1. `fbird_query("SET TRANSACTION...")` called
2. Extension creates implicit default transaction (T1) to prepare SQL
3. `SET TRANSACTION` creates explicit transaction (T2)
4. Nested transactions cause deadlock

**Solution (for doctrine driver):**
- Use `fbird_trans($conn, $flags)` API directly
- This bypasses SQL parsing and implicit transaction creation
- Maps isolation levels to `FBIRD_*` constants

**Action:** No php-firebird changes needed. The `fbird_trans()` API already supports all necessary flags.

---

## Release Verification Checklist

### Pre-Release Testing

- [ ] **CI Matrix:** All 20 combinations pass (PHP 8.1-8.5 × Firebird 2.5-5.0)
- [ ] **Exception Mode Tests:** Both `fbird_exception_mode_00{1,2}.phpt` pass
- [ ] **Fork-safety Test:** `issue22_pcntl_fork_001.phpt` passes
- [ ] **PHPStan:** Level 8 clean (0 errors)
- [ ] **PHPCS:** PSR-12 clean (0 violations)
- [ ] **Existing Tests:** No regressions in existing test suite

### Local Verification Steps

```bash
# 1. Run full test suite with act (local CI)
act -W .github/workflows/main.yml

# 2. Run specific feature tests
docker-compose exec php run-tests.php tests/fbird_exception_mode_001.phpt
docker-compose exec php run-tests.php tests/fbird_exception_mode_002.phpt
docker-compose exec php run-tests.php tests/issue22_pcntl_fork_001.phpt

# 3. PHPStan analysis
vendor/bin/phpstan analyse src/ --level=8

# 4. PHPCS check
vendor/bin/phpcs src/
```

### Release Steps

1. **Verify all tests pass** (local + CI)
2. **Update CHANGELOG.md:** Move Unreleased items to [7.0.0-rc.6] section
3. **Tag release:** `git tag v7.0.0-rc.6`
4. **Push:** `git push origin v7.0.0-rc.6`
5. **Create GitHub release** with release notes

---

## Release Notes Draft

```markdown
## [7.0.0-rc.6] - 2025-12-2X

### Added

- **Exception Mode API (#15)**: PDO-style exception handling for clean error management
  - `fbird_set_exception_mode(int $mode): bool` - Set runtime exception mode
  - `fbird_get_exception_mode(): int` - Get current exception mode  
  - Constants: `FBIRD_EXCEPTION_MODE_SILENT` (0), `FBIRD_EXCEPTION_MODE_THROW` (1)
  - `Firebird\Exception::getSqlState(): string` - Return SQLSTATE error code
  - Required for Doctrine DBAL integration (PDO::ERRMODE_EXCEPTION compatibility)

### Fixed

- **Issue #22 (Fork-safety)**: Extension no longer segfaults in forked processes
  - PID tracking detects forked children
  - Resource destructors skip Firebird API cleanup in children
  - Enables PHPStan parallel, PHPUnit parallel, pcntl_fork usage
```

---

## Post-Release Tasks

### Notification

- [ ] Update doctrine-firebird-driver issue #33 comment with rc.6 release info
- [ ] Update satwareAG/php-firebird README with Exception Mode documentation

### Documentation

- [ ] Add Exception Mode examples to `docs/EXAMPLES.md`
- [ ] Update README.md with new features

### Future Considerations (post-rc.6)

| Item | Priority | Notes |
|------|----------|-------|
| Benchmark suite expansion | Low | Upstream #83 |
| PECL publishing | Low | Upstream #12 |
| PHP.net documentation PRs | Low | Upstream #63, #72, #90 |

---

## Related Issues

| Repository | Issue | Status |
|------------|-------|--------|
| satwareAG/php-firebird | #15 | ✅ Implemented (rc.6) |
| satwareAG/php-firebird | #22 | ✅ Implemented (rc.6) |
| satwareAG/doctrine-firebird-driver | #33 | 📝 Driver-side action |
| satwareAG/doctrine-firebird-driver | #23 | 📝 Driver-side fix |
| FirebirdSQL/php-firebird | 12 fixed | ✅ All in fork |

---

## Appendix: Exception Mode Implementation Notes

### INI Setting Interaction

The `fbird.enable_exceptions` INI setting provides default behavior:
- `fbird.enable_exceptions = 0` → Default SILENT mode
- `fbird.enable_exceptions = 1` → Default THROW mode

Runtime `fbird_set_exception_mode()` **overrides** INI setting for current request.

### SQLSTATE Mapping

| SQLSTATE | Meaning | Firebird Error Class |
|----------|---------|---------------------|
| 23000 | Integrity constraint violation | Foreign key, unique violation |
| 42000 | Syntax error or access rule violation | Invalid SQL, permissions |
| HY000 | General error | Other errors |
| 08001 | Connection exception | Connection failures |
| 40001 | Serialization failure | Deadlock/conflict |

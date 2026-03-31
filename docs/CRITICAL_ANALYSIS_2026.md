# php-firebird Critical Analysis - March 2026

Post-v10.3.6 comprehensive review against 2026 best practices for PHP extension
development, C/C++ Linux development, and cross-platform distribution.

## Executive Summary

The php-firebird extension is a **mature, well-engineered** project with excellent
test coverage (274 .phpt tests, 97.7% API coverage), comprehensive CI/CD (12-target
matrix, sanitizers, CodeQL, coverage), and a clean C++17/C architecture. It is
significantly ahead of most PHP extensions in tooling and safety infrastructure.

**However**, the audit identified **2 critical security issues**, **7 high-priority
improvements**, and **~20 medium/low enhancements** across build system, code quality,
CI/CD, testing, and documentation.

---

## Findings Summary

| Severity | Count | Key Areas |
|----------|-------|-----------|
| **Critical** | 2 | SQL injection in `fbird_create_database()`, SPB buffer overflow |
| **High** | 7 | No artifact signing, no SBOM, missing hardening flags, stale CONTRIBUTING.md, no dependabot, missing `--CLEAN--` sections, no ARM64/musl builds |
| **Medium** | 12 | C standard not pinned, arginfo modernization, resource-to-object migration incomplete, version stamps stale, missing TSan, no fuzz dictionary |
| **Low/Info** | 10+ | LTO, config.m4 PHP version check, dead code, doc cross-refs |

---

## 1. CRITICAL: Security

### C1. SQL Injection in `fbird_create_database()`

**File**: `fbird_connection.c:598-615`

```c
int pos = snprintf(create_sql, sizeof(create_sql), "CREATE DATABASE '%s'", database);
if (username && username_len > 0) {
    pos += snprintf(create_sql + pos, ..., " USER '%s'", username);
}
if (password && password_len > 0) {
    pos += snprintf(create_sql + pos, ..., " PASSWORD '%s'", password);
}
```

User-supplied `database`, `username`, `password` are interpolated directly into DDL
SQL without escaping single quotes. A database path containing `'` enables injection
of arbitrary DDL clauses. The `charset` parameter is inserted without quotes, allowing
injection of arbitrary DDL keywords.

**Impact**: An attacker controlling the database path or credentials can inject arbitrary
Firebird DDL (e.g., `CREATE DATABASE 'a' USER 'x'; DROP DATABASE 'other'`).

**Fix**: Escape single quotes (`'` to `''`) in all interpolated values before building
the SQL string. Consider using the Firebird API's `dpb` parameter block instead of SQL
string construction where possible.

**Priority**: P0 - Fix before next release.

### C2. SPB Buffer Overflow in `Firebird\Service::__construct()`

**File**: `fbird_classes.c:856-870`

```c
char buf[256];
int buf_len = 0;
buf[buf_len++] = isc_spb_version;
buf[buf_len++] = isc_spb_current_version;
buf[buf_len++] = isc_spb_user_name;
buf[buf_len++] = (char)user_len;  // truncation if user_len > 255
memcpy(buf + buf_len, user, user_len); buf_len += user_len;
buf[buf_len++] = isc_spb_password;
buf[buf_len++] = (char)pass_len;  // truncation if pass_len > 255
memcpy(buf + buf_len, pass, pass_len); buf_len += pass_len;
```

Two issues:
1. **Stack buffer overflow**: If `user_len + pass_len > ~248`, the `memcpy` writes
   past the 256-byte `buf[]` - classic stack buffer overflow.
2. **Length truncation**: `(char)user_len` truncates lengths > 255 to a single byte,
   causing the SPB length field to be wrong.

Additionally, `char loc[256]` with `snprintf(loc, sizeof(loc), "%s:service_mgr", host)`
is safe due to `snprintf` truncation, but a long hostname will be silently truncated.

**Fix**: Validate `user_len` and `pass_len` with bounds checks before buffer construction.
Use dynamic allocation (`emalloc`) sized to actual content, or reject inputs > 255 bytes
(Firebird SPB protocol limit).

**Priority**: P0 - Fix before next release.

---

## 2. HIGH Priority

### H1. No Artifact Signing or Attestation

**Files**: `release-linux.yml`, `release-windows.yml`

Neither Linux nor Windows release workflows sign precompiled `.so`/`.dll` artifacts.
No Cosign, Sigstore, or `gh attestation` exists. SHA256 checksums provide integrity
but not authenticity. For a C extension loaded into PHP at runtime, this is a
significant supply chain risk.

**Fix**: Add `gh attestation generate` or `cosign sign-blob` step to release workflows.
Use GitHub's built-in SLSA provenance attestation (free for public repos).

### H2. No SBOM Generation

No Software Bill of Materials (CycloneDX/SPDX) for precompiled bundles. Bundled
Firebird libraries (libfbclient, libicu*, libtommath, libtomcrypt, libre2) are
distributed without tracking. NIS2/EU compliance requires SBOM for distributed software.

**Fix**: Add `syft` or `cdxgen` step to release workflows generating CycloneDX JSON
alongside each release artifact.

### H3. No Dependabot/Renovate for GitHub Actions

All 12 action references are SHA-pinned (excellent), but without automated update
tooling. The `actions/upload-artifact@v5` still uses Node.js 20, deprecated June 2026.

**Fix**: Add `.github/dependabot.yml`:

```yaml
version: 2
updates:
  - package-ecosystem: github-actions
    directory: /
    schedule:
      interval: weekly
```

### H4. Missing Compiler Hardening Flags in config.m4

**File**: `config.m4:115`

The `PHP_NEW_EXTENSION` call only passes `-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1`.
Missing security-critical flags:

| Flag | Purpose |
|------|---------|
| `-Wall -Wextra` | Comprehensive warnings |
| `-D_FORTIFY_SOURCE=2` | Buffer overflow detection |
| `-fstack-protector-strong` | Stack smashing protection |
| `-Wformat -Wformat-security` | Format string safety |

PHP's build system adds some flags, but explicitly adding hardening flags ensures
consistent coverage across all build environments including user builds.

**Fix**: Add flags to `PHP_NEW_EXTENSION` extra flags parameter or via
`PHP_ADD_MAKEFILE_FRAGMENT`.

### H5. CONTRIBUTING.md Has Critical Stale Information

**File**: `CONTRIBUTING.md`

| Line | Issue | Correct Value |
|------|-------|---------------|
| 3, 8 | "PHP 8.1+" | PHP 8.2+ (8.1 dropped v7.2.0) |
| 10 | "Firebird 2.5, 3.0, 4.0, 5.0+" | 3.0, 4.0, 5.0+ (2.5 dropped v7.2.0) |
| 98 | `github.com/FirebirdSQL/php-firebird` | `github.com/satwareAG/php-firebird` |
| 300 | Test envs include PHP 8.1, FB 2.5 | Remove both |

New contributors will clone the wrong repository and target wrong versions.

**Fix**: Update all version references and the clone URL.

### H6. Missing `--CLEAN--` Sections in .phpt Tests

Nearly all 274 .phpt tests lack `--CLEAN--` sections. Tests create tables, databases,
and other persistent artifacts but rely on `common.inc` teardown. If a test fails
mid-execution, artifacts persist and can cause cascading failures in subsequent tests.

**Fix**: Add `--CLEAN--` sections to all tests that create database objects.
Start with the 40 coverage tests and 45 PDO tests.

### H7. No ARM64 or Alpine/musl Precompiled Builds

Release pipeline only produces x86_64 Linux (manylinux_2_28) and x86_64 Windows
builds. No ARM64/aarch64 or Alpine/musl-libc builds exist, despite growing ARM64
server adoption (AWS Graviton, Ampere).

**Fix**: Add `linux/arm64` targets to `release-linux.yml` matrix. Consider
`musllinux_1_2` for Alpine users.

---

## 3. MEDIUM Priority

### M1. C Standard Not Explicitly Selected

**File**: `config.m4`

No `-std=c17` or `-std=gnu17` flag for C source files. Compiles with whatever the
system default is (varies by GCC/Clang version). This causes inconsistent behavior
across build environments.

C++17 is correctly enforced via `PHP_CXX_COMPILE_STDCXX([17], [mandatory])`.

**Fix**: Add `-std=gnu17` to C compilation flags in `config.m4`.

### M2. Arginfo Modernization Incomplete

**File**: `firebird.c`

| Pattern | Count |
|---------|-------|
| `ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO` (modern) | 15 |
| `ZEND_BEGIN_ARG_INFO_EX` (legacy, no return type) | 84 |

84 out of 99 arginfo declarations use the legacy `ZEND_BEGIN_ARG_INFO_EX` macro
that doesn't declare return types. PHP 8.x best practice is to use
`ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX` for all functions to enable Reflection
and IDE autocompletion of return types.

The OOP layer (`fbird_classes.c`) correctly uses the modern pattern - the gap is
in the procedural `fbird_*` functions.

**Fix**: Migrate all 84 procedural arginfo declarations to typed variants. Can be
done incrementally per function group.

### M3. Resource-to-Object Migration Incomplete

The extension still uses `zend_resource` (PHP resources) as primary handles for
connections, transactions, queries, and blobs. PHP 8.x deprecates resources in favor
of opaque objects. While Layer 2 OOP classes exist (`Firebird\Connection`,
`Firebird\Transaction`, etc.), Layer 1 procedural functions still return/accept
resources.

PHP upstream extensions (ext/pgsql, ext/mysqli) have been migrating to objects.
Resources may be removed in a future PHP version.

**Fix**: Long-term migration to return opaque `\Firebird\Connection` objects from
`fbird_connect()` etc. The OOP layer already wraps resources - the procedural layer
needs to follow. This is a major version change (v11.0).

### M4. Version Stamps Stale Across Stubs

| File | Version Stamp | Actual |
|------|---------------|--------|
| `stubs/firebird-stubs.php` | `@version 9.0.0` | 10.3.6 |
| `stubs/firebird-classes.php` | `@version 10.0.0` | 10.3.6 |
| `stubs/pdo-fbird-stubs.php` | `@version 10.0.0` | 10.3.6 |
| `stubs/composer.json` branch-alias | Needs verification | 10.3.x |

**Fix**: Add a CI check or release script that validates version stamps match `VERSION`.

### M5. No TSan (Thread Sanitizer) Testing

ASAN, UBSan, and LeakSanitizer are all used, but TSan is missing. For ZTS (Zend
Thread Safety) builds, thread safety issues could exist in global state access
(`IBG()` macro, `master_instance`). PHP's `RTLD_DEEPBIND` on Linux interferes with
ASan but not TSan.

**Fix**: Add a TSan job to `sanitizers.yml` with a ZTS PHP build.

### M6. No Fuzz Dictionary

**File**: `fuzz/`

The fuzzer has corpus files but no dictionary. A Firebird SQL dictionary
(keywords, operators, special characters) dramatically improves fuzzing
effectiveness for SQL parsing paths.

**Fix**: Create `fuzz/dictionary/sql.dict` with Firebird SQL keywords.

### M7. config.m4 PHP Version Check Says 8.1 (Should Be 8.2)

**File**: `config.m4:27`

```m4
AC_MSG_CHECKING([for minimum PHP version 8.1])
```

The extension requires PHP 8.2+ but the config check allows 8.1.

**Fix**: Update to `8.2`.

### M8. Pointer Smuggling via `IBG(status[])`

**File**: `fbird_connection.c:249-251`

```c
IBG(status[ISC_STATUS_LENGTH - 1]) = (ISC_STATUS)(uintptr_t)connection;
```

A `void*` pointer is stored in an `ISC_STATUS` (typically `intptr_t`) slot as a
hack to pass the connection pointer from `_php_fbird_attach_db()` to
`_php_fbird_connect()`. This works but is fragile and violates type safety.

**Fix**: Return the connection pointer via a dedicated output parameter or struct
field instead of abusing the status vector.

### M9. `fbp_error_ex()` Uses Fixed 1024-byte Buffer

**File**: `firebird.c` (near end)

```c
void fbp_error_ex(long level, const char *msg, ...)
{
    char buf[1024] = {0};
    vsnprintf(buf, sizeof(buf), msg, ap);
```

Error messages > 1023 chars are silently truncated. While unlikely in practice,
Firebird can return long error chains.

**Fix**: Use `vspprintf` (Zend's allocating variant) for unbounded error messages.

### M10. `fbird_create_database()` Uses 4096-byte Stack Buffer

**File**: `fbird_connection.c:593`

```c
char create_sql[4096];
```

While `snprintf` prevents overflow, a very long database path + username + password
+ charset could cause silent truncation of the SQL statement.

**Fix**: Use `spprintf` (Zend's allocating snprintf) for dynamic sizing.

### M11. macOS Build Not Tested in CI

No macOS runner in any workflow. `config.m4` should work on macOS with Homebrew
Firebird, but this is untested. Apple Silicon (ARM64) is particularly important.

**Fix**: Add a macOS job to CI (at least build-only, Firebird server unavailable).

### M12. OOP Layer 2 Delegates to Layer 1 via `call_user_function()`

**File**: `fbird_classes.c`

`Firebird\Connection::__construct()` calls `fbird_connect()` via `call_user_function()`,
and `Firebird\Connection::prepare()` calls `fbird_prepare()` the same way. This adds
PHP function call overhead and makes the OOP layer dependent on the procedural layer's
exact signatures.

**Fix**: Long-term, call the internal C functions directly instead of going through
the PHP function dispatch mechanism.

---

## 4. LOW Priority / Informational

### L1. No LTO (Link-Time Optimization) for Release Builds

Precompiled release builds don't use `-flto`. For a native extension, LTO can provide
5-15% performance improvement with no behavioral change.

### L2. `config.w32` References Legacy `gds32_ms`

Windows config checks for `gds32_ms.lib` as a fallback - this is Firebird 1.x era.
Safe to remove for a 3.0+ minimum.

### L3. Legacy `FB_API_VER >= 30` Guards Are Now Dead Code

Since Firebird 3.0 is the minimum, all `#if FB_API_VER >= 30` blocks are always true.
The `#else` branches are dead code that can be removed.

### L4. Mixed `emalloc`/`ecalloc` Patterns

Some allocations use `emalloc` followed by manual zeroing, others use `ecalloc`.
Should standardize on `ecalloc` for structs to prevent uninitialized field bugs.

### L5. No Architecture Decision Records

No `docs/adr/` directory. Key decisions (OO API migration, ibase_ removal, PDO
integration strategy) are documented in specs but not in formal ADR format.

### L6. `hash_key` Comparison Uses `memcmp` Against Null Bytes

**File**: `fbird_connection.c:139`

```c
if (link->hash_key[0] != '\0' || memcmp(link->hash_key, "\0\0...", 16) != 0)
```

The first check (`!= '\0'`) short-circuits in most cases, but the fallback `memcmp`
against 16 null bytes is redundant - if `hash_key[0]` is `\0` and the rest isn't
null, this is suspicious. Consider using a boolean flag `has_hash_key` instead.

### L7. Valgrind Suppressions Are Comprehensive

The `valgrind-php.supp` file has well-documented suppressions for known PHP engine
false positives. This is above-average quality for PHP extensions.

### L8. Missing `FBIRD_CONNECT_FORCE_NEW` Constant Documentation

The `flags` parameter to `fbird_connect()` accepts `FBIRD_CONNECT_FORCE_NEW` but
this isn't documented in stubs or docs.

### L9. Events Timeout Test Deferred (Known Issue)

RC4-era deferred test for events with timeout needs `pcntl_alarm` or a C-level
timeout mechanism. This is tracked but not resolved.

### L10. Stubs Sync Script Is Excellent

`scripts/check-stubs-sync.sh` is a well-implemented guard against stub drift.
The `.github/workflows/split-stubs.yml` auto-sync to `satwareAG/php-firebird-stubs`
is a good distribution pattern.

---

## 5. Strengths (What's Done Well)

| Area | Assessment |
|------|------------|
| **Test coverage** | 274 tests, 97.7% API function coverage - exceptional for a PHP C extension |
| **CI matrix** | 12-target PHP×Firebird matrix with daily nightly builds |
| **Memory safety** | ASAN + UBSan + Valgrind + LeakSanitizer in CI |
| **Static analysis** | clang-tidy, cppcheck, PHPStan Level 8, CodeQL |
| **Action pinning** | All 12 GitHub Actions SHA-pinned with version comments |
| **Permissions** | `read-all` at workflow level, scoped write per-job |
| **C++17 integration** | Clean RAII wrappers with C interop layer |
| **Fork safety** | Two-level PID checking (global + per-connection) |
| **OOP layer** | Full `Firebird\*` class hierarchy with proper Zend object handlers |
| **Fuzzing** | PHP-based fuzzer with corpus and ASAN integration |
| **Documentation** | 600+ line README, comprehensive CHANGELOG, examples |
| **Stubs sync** | Automated sync check + split-stubs workflow |
| **PDO driver** | Integrated `pdo_fbird` with conditional compilation |
| **Windows support** | Full Windows DLL build pipeline with php-windows-builder |

---

## 6. Prioritized Improvement Roadmap

### Phase 1: Security (v10.3.7 - Immediate)

- [ ] **C1**: Fix SQL injection in `fbird_create_database()` - escape quotes
- [ ] **C2**: Fix SPB buffer overflow in `Firebird\Service::__construct()` - bounds check
- [ ] **M10**: Replace `create_sql[4096]` with `spprintf` dynamic allocation
- [ ] **H5**: Update CONTRIBUTING.md (wrong repo URL, wrong PHP/FB versions)
- [ ] **M7**: Fix config.m4 PHP version check (8.1 -> 8.2)

### Phase 2: Supply Chain (v10.4.0 - Next Minor)

- [ ] **H1**: Add artifact signing (`gh attestation generate`) to release workflows
- [ ] **H2**: Add SBOM generation (`syft`) to release workflows
- [ ] **H3**: Add `.github/dependabot.yml` for action updates
- [ ] **M4**: Automate version stamp sync in stubs during release

### Phase 3: Build Hardening (v10.4.x)

- [ ] **H4**: Add `-Wall -Wextra -D_FORTIFY_SOURCE=2 -fstack-protector-strong`
- [ ] **M1**: Pin C standard to `-std=gnu17`
- [ ] **M8**: Replace pointer smuggling with proper output parameter
- [ ] **L1**: Add LTO for release builds (`-flto=auto`)

### Phase 4: Testing (v10.5.0)

- [ ] **H6**: Add `--CLEAN--` sections to all .phpt tests (start with coverage + PDO)
- [ ] **M5**: Add TSan job for ZTS builds
- [ ] **M6**: Create fuzz dictionary for Firebird SQL
- [ ] **M11**: Add macOS CI job (build-only)

### Phase 5: Platform Expansion (v10.6.0 or v11.0)

- [ ] **H7**: Add ARM64 Linux precompiled builds
- [ ] Add Alpine/musl-libc builds
- [ ] Add macOS precompiled builds (universal binary)

### Phase 6: Modernization (v11.0 - Next Major)

- [ ] **M2**: Migrate all 84 arginfo declarations to typed variants
- [ ] **M3**: Begin resource-to-object migration for Layer 1
- [ ] **M12**: Replace `call_user_function()` in OOP layer with direct C calls
- [ ] **L3**: Remove dead `FB_API_VER < 30` code paths
- [ ] **L2**: Remove `gds32_ms` fallback from config.w32

---

## Methodology

Analysis performed 2026-03-31 using parallel sub-agent exploration across 5 domains:
1. Build system (`config.m4`, `config.w32`, Dockerfile, build scripts)
2. C/C++ code quality (all `.c`/`.cpp`/`.h`/`.hpp` files)
3. Testing infrastructure (274 .phpt tests, fuzzer, sanitizers, Docker matrix)
4. CI/CD & distribution (8 GitHub Actions workflows, release pipelines)
5. Documentation & stubs (README, CONTRIBUTING, stubs, PHPStan config)

Each domain was analyzed against 2026 best practices for PHP extension development
(PHP 8.4+ patterns), C/C++ development (C17/C++20 standards), and supply chain
security (SLSA, NIS2, Sigstore).

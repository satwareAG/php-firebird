# PHP Firebird Extension Versioning Strategy

**Document Version:** 1.0  
**Last Updated:** 2025-12-24  
**Status:** Proposed

## Executive Summary

This document outlines the versioning strategy for the `php-firebird` extension, transitioning from hardcoded version strings to dynamic Git-based version generation while maintaining reproducibility for release artifacts.

## Current State

**Location:** `php_firebird.h` (lines 17-27)

```c
#define PHP_FIREBIRD_VER_MAJOR 1
#define PHP_FIREBIRD_VER_MINOR 0
#define PHP_FIREBIRD_VER_REV 0
/* #define PHP_FIREBIRD_VER_PRE "-RC2" -- Defined only for pre-releases */

#define PHP_FIREBIRD_VER PHP_FIREBIRD_VER_MAJOR * 10 + PHP_FIREBIRD_VER_MINOR

#ifdef PHP_FIREBIRD_VER_PRE
#   define PHP_FIREBIRD_VER_STR "1.0.0" PHP_FIREBIRD_VER_PRE
#else
#   define PHP_FIREBIRD_VER_STR "1.0.0"
#endif
```

**Problems:**
- Manual updates required for every release
- No connection between git tags and compiled version
- High risk of version mismatch errors
- Developers must remember to update multiple locations

## Proposed Strategy: Git-Based Dynamic Versioning

### 1. Version Source Hierarchy

| Priority | Source | Use Case | Example |
|----------|--------|----------|---------|
| 1 | Git tags | Development builds from git | `7.0.0-rc.7-15-g53d4877-dirty` |
| 2 | `VERSION` file | Release tarballs (no `.git/`) | `7.0.0-rc.7` |
| 3 | Fallback | Build errors/unknown state | `0.0.0-unknown` |

### 2. Implementation Mechanism

**Build-Time Version Generation** (via `config.m4`):

```m4
# Version detection during ./configure
AC_MSG_CHECKING([for php-firebird version])

if test -d "$srcdir/.git" -a -x "`which git 2>/dev/null`"; then
  # Git repository: Use git describe
  PHP_FIREBIRD_VERSION=`cd "$srcdir" && git describe --tags --always --dirty 2>/dev/null`
  if test -z "$PHP_FIREBIRD_VERSION"; then
    PHP_FIREBIRD_VERSION="0.0.0-unknown"
  fi
elif test -f "$srcdir/VERSION"; then
  # Release tarball: Read VERSION file
  PHP_FIREBIRD_VERSION=`cat "$srcdir/VERSION"`
else
  # Fallback
  PHP_FIREBIRD_VERSION="0.0.0-unknown"
fi

AC_MSG_RESULT([$PHP_FIREBIRD_VERSION])
AC_DEFINE_UNQUOTED([PHP_FIREBIRD_VERSION_STRING], ["$PHP_FIREBIRD_VERSION"], 
                    [PHP Firebird extension version])
```

**Parse version components for numeric macros:**

```m4
# Extract major.minor.patch from version string
# Input: "7.0.0-rc.7-15-g53d4877" or "7.0.0"
PHP_FIREBIRD_VER_MAJOR=`echo $PHP_FIREBIRD_VERSION | sed 's/^\([0-9]*\).*/\1/'`
PHP_FIREBIRD_VER_MINOR=`echo $PHP_FIREBIRD_VERSION | sed 's/^[0-9]*\.\([0-9]*\).*/\1/'`
PHP_FIREBIRD_VER_PATCH=`echo $PHP_FIREBIRD_VERSION | sed 's/^[0-9]*\.[0-9]*\.\([0-9]*\).*/\1/'`

# Default to 0 if extraction fails
test -z "$PHP_FIREBIRD_VER_MAJOR" && PHP_FIREBIRD_VER_MAJOR=0
test -z "$PHP_FIREBIRD_VER_MINOR" && PHP_FIREBIRD_VER_MINOR=0
test -z "$PHP_FIREBIRD_VER_PATCH" && PHP_FIREBIRD_VER_PATCH=0

AC_DEFINE_UNQUOTED([PHP_FIREBIRD_VER_MAJOR], [$PHP_FIREBIRD_VER_MAJOR], 
                    [Major version number])
AC_DEFINE_UNQUOTED([PHP_FIREBIRD_VER_MINOR], [$PHP_FIREBIRD_VER_MINOR], 
                    [Minor version number])
AC_DEFINE_UNQUOTED([PHP_FIREBIRD_VER_PATCH], [$PHP_FIREBIRD_VER_PATCH], 
                    [Patch version number])
```

**Update `php_firebird.h`:**

```c
#ifndef PHP_FIREBIRD_H
#define PHP_FIREBIRD_H

/* Version components defined by configure script via -D flags */
#ifndef PHP_FIREBIRD_VER_MAJOR
#  define PHP_FIREBIRD_VER_MAJOR 0
#endif
#ifndef PHP_FIREBIRD_VER_MINOR
#  define PHP_FIREBIRD_VER_MINOR 0
#endif
#ifndef PHP_FIREBIRD_VER_PATCH
#  define PHP_FIREBIRD_VER_PATCH 0
#endif

/* Full version string defined by configure script */
#ifndef PHP_FIREBIRD_VERSION_STRING
#  define PHP_FIREBIRD_VERSION_STRING "0.0.0-unknown"
#endif

/* Numeric version for comparisons (MMmmpp format) */
#define PHP_FIREBIRD_VER_NUM \
  (PHP_FIREBIRD_VER_MAJOR * 10000 + \
   PHP_FIREBIRD_VER_MINOR * 100 + \
   PHP_FIREBIRD_VER_PATCH)

/* Legacy compatibility (deprecated, use PHP_FIREBIRD_VERSION_STRING) */
#define PHP_FIREBIRD_VER_STR PHP_FIREBIRD_VERSION_STRING

/* ... rest of header ... */
#endif
```

### 3. VERSION File Management

**Purpose:** Provide version string for release tarballs without `.git/` directory.

**Creation (during release builds):**

```bash
# Automated by GitHub Actions / release script
git describe --tags --abbrev=0 > VERSION
# OR for exact commit
git describe --tags > VERSION
```

**Content Examples:**

```
# Clean release
7.0.0

# Release candidate
7.0.0-rc.7

# Pre-release snapshot (if needed)
7.0.0-rc.7-15-g53d4877
```

**.gitignore:**

```
# VERSION file is generated, not committed to git
/VERSION
```

**EXCEPTION:** For official release tarballs, `VERSION` file MUST be included in the distributed package.

### 4. Git Tagging Strategy (SemVer 2.0.0)

**Format:** `vMAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]`

**Tag Examples:**

```bash
# Stable releases
git tag -a v7.0.0 -m "Release 7.0.0"

# Release candidates
git tag -a v7.0.0-rc.7 -m "Release Candidate 7 for v7.0.0"

# Alpha/Beta
git tag -a v7.1.0-alpha.1 -m "Alpha 1 for v7.1.0"
git tag -a v7.1.0-beta.2 -m "Beta 2 for v7.1.0"
```

**`git describe` Output:**

```bash
# On exact tag
$ git describe --tags
v7.0.0-rc.7

# 15 commits after tag
$ git describe --tags
v7.0.0-rc.7-15-g53d4877

# Uncommitted changes
$ git describe --tags --dirty
v7.0.0-rc.7-15-g53d4877-dirty
```

### 5. PHP Extension Metadata

**`phpinfo()` Output:**

```
php-firebird

Firebird client version => Firebird 5.0.0.1305
Extension version => 7.0.0-rc.7-15-g53d4877
...
```

**Implementation in `firebird.c`:**

```c
PHP_MINFO_FUNCTION(fbird)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "Firebird Support", "enabled");
    php_info_print_table_row(2, "Extension version", PHP_FIREBIRD_VERSION_STRING);
    php_info_print_table_row(2, "Compiled against", FIREBIRD_CLIENT_VERSION);
    /* ... other info ... */
    php_info_print_table_end();
}
```

### 6. Release Workflow

**Development Build (from git):**

```bash
git clone https://github.com/satwareAG/php-firebird.git
cd php-firebird
phpize
./configure --with-firebird
make
# Version automatically detected from git describe
```

**Release Tarball Creation:**

```bash
# 1. Tag the release
git tag -a v7.0.0 -m "Release 7.0.0"
git push origin v7.0.0

# 2. Generate VERSION file
git describe --tags --abbrev=0 > VERSION

# 3. Create release tarball
git archive --format=tar.gz --prefix=php-firebird-7.0.0/ \
  -o php-firebird-7.0.0.tar.gz HEAD

# 4. Add VERSION file to tarball
tar -rf php-firebird-7.0.0.tar --transform='s|^|php-firebird-7.0.0/|' VERSION
gzip -f php-firebird-7.0.0.tar
```

**User Build (from tarball):**

```bash
tar xzf php-firebird-7.0.0.tar.gz
cd php-firebird-7.0.0
phpize
./configure --with-firebird
make
# Version read from VERSION file
```

### 7. Compatibility Considerations

**PHP Extension Versioning:**

PHP extensions expose version via `zend_module_entry`:

```c
zend_module_entry firebird_module_entry = {
    STANDARD_MODULE_HEADER,
    "firebird",                          /* Extension name */
    ext_functions,                       /* Functions */
    PHP_MINIT(fbird),                    /* Module init */
    PHP_MSHUTDOWN(fbird),                /* Module shutdown */
    PHP_RINIT(fbird),                    /* Request init */
    PHP_RSHUTDOWN(fbird),                /* Request shutdown */
    PHP_MINFO(fbird),                    /* Module info */
    PHP_FIREBIRD_VERSION_STRING,         /* Version string (dynamic) */
    STANDARD_MODULE_PROPERTIES
};
```

**Composer/PECL Integration:**

- **Composer:** Uses `ext-firebird: *` or `ext-firebird: ^7.0` constraints
- **PECL:** Version from `package.xml` (manually maintained for releases)
- **Note:** `package.xml` must match git tag for PECL releases

### 8. Best Practices (2025)

**1. Reproducible Builds:**

- **Deterministic Versions:** Release tarballs MUST include `VERSION` file
- **Build Info:** Store build timestamp, git commit hash in `phpinfo()`
- **Toolchain:** Document compiler version, flags in build artifacts

**2. Semantic Versioning (SemVer 2.0.0):**

- **MAJOR:** Breaking API changes (e.g., function signature changes)
- **MINOR:** New features, backward-compatible (e.g., new functions)
- **PATCH:** Bug fixes, no API changes
- **Pre-release:** Use `-alpha.N`, `-beta.N`, `-rc.N` suffixes

**3. Automated Release Management:**

```yaml
# .github/workflows/release.yml
name: Create Release
on:
  push:
    tags:
      - 'v*'
jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Generate VERSION file
        run: git describe --tags --abbrev=0 > VERSION
      - name: Create tarball
        run: |
          VERSION_TAG=${GITHUB_REF#refs/tags/v}
          git archive --format=tar.gz --prefix=php-firebird-${VERSION_TAG}/ \
            -o php-firebird-${VERSION_TAG}.tar.gz HEAD
          tar -rf php-firebird-${VERSION_TAG}.tar \
            --transform="s|^|php-firebird-${VERSION_TAG}/|" VERSION
          gzip -f php-firebird-${VERSION_TAG}.tar
      - name: Create GitHub Release
        uses: softprops/action-gh-release@v1
        with:
          files: php-firebird-*.tar.gz
          generate_release_notes: true
```

**4. Version Validation:**

```bash
# CI pipeline step: Verify version consistency
#!/bin/bash
set -e

# Get version from different sources
GIT_VERSION=$(git describe --tags --always)
HEADER_VERSION=$(grep 'PHP_FIREBIRD_VERSION_STRING' php_firebird.h | \
                 sed 's/.*"\(.*\)".*/\1/')
PECL_VERSION=$(grep '<version>' package.xml | head -1 | \
               sed 's/.*<version>\(.*\)<\/version>.*/\1/')

echo "Git version: $GIT_VERSION"
echo "Header version: $HEADER_VERSION"
echo "PECL version: $PECL_VERSION"

# Verify consistency (for release builds)
if [[ "$CI_COMMIT_TAG" != "" ]]; then
  if [[ "$PECL_VERSION" != "${CI_COMMIT_TAG#v}" ]]; then
    echo "ERROR: PECL version mismatch"
    exit 1
  fi
fi
```

**5. Developer Experience:**

- **Local Builds:** Version changes automatically on git checkout
- **Pull Requests:** Version includes PR branch info (`v7.0.0-rc.7-PR123-15-g53d4877`)
- **Dirty Working Tree:** `-dirty` suffix warns uncommitted changes

## Migration Plan

### Phase 1: Add Dynamic Versioning (Non-Breaking)

1. Update `config.m4` with version detection logic
2. Add `VERSION` file generation to release scripts
3. Update `php_firebird.h` to use configure-defined macros
4. Test with both git and tarball builds
5. Document in `CONTRIBUTING.md`

**Status:** ✅ Implemented (2025-12-24)  
**Target:** v7.0.0 or v7.1.0

### Phase 2: Deprecate Manual Version Updates

1. Add CI check for version consistency
2. Update release documentation
3. Remove manual version from `php_firebird.h` (keep fallbacks)
4. Enforce via pre-commit hooks

**Status:** Planned  
**Target:** v7.2.0

### Phase 3: Automation Excellence

1. Full GitHub Actions integration
2. Automated PECL package generation
3. Version compatibility matrix
4. Release notes automation

**Status:** Future enhancement  
**Target:** v8.0.0

## Testing Strategy

### Test Matrix

| Scenario | Git Repo | VERSION File | Expected Version |
|----------|----------|--------------|------------------|
| Dev build (git) | ✓ | ✗ | `v7.0.0-rc.7-15-g53d4877-dirty` |
| Clean tag | ✓ (on tag) | ✗ | `v7.0.0-rc.7` |
| Release tarball | ✗ | ✓ | `7.0.0-rc.7` |
| Broken build | ✗ | ✗ | `0.0.0-unknown` |

### Validation Commands

```bash
# After compilation, verify version
php -r "echo phpversion('firebird');"
php -i | grep "Extension version"

# Programmatic check
php -r "if (version_compare(phpversion('firebird'), '7.0.0', '>=')) echo 'OK';"
```

## References

### Industry Standards

- **SemVer 2.0.0:** https://semver.org/spec/v2.0.0.html
- **Reproducible Builds:** https://reproducible-builds.org/
- **Git Describe:** https://git-scm.com/docs/git-describe

### Related Projects

- **PHP Core:** Uses git tags + manual updates in `main/php_version.h`
- **libfbclient:** Firebird uses `#define FB_API_VER` (build-time constant)
- **PECL Extensions:** Mixed approaches (Redis: git-based, Xdebug: manual)

### PHP Extension Guidelines

- **PECL Documentation:** https://pecl.php.net/
- **Extension Development:** https://www.php.net/manual/en/internals2.php
- **Module Entry:** https://www.phpinternals.net/docs/zend_module_entry

## Appendix: Example Output

### Development Build

```bash
$ git describe --tags --dirty
v7.0.0-rc.7-15-g53d4877-dirty

$ php -i | grep firebird
firebird
Extension version => v7.0.0-rc.7-15-g53d4877-dirty
```

### Release Build

```bash
$ cat VERSION
7.0.0

$ php -i | grep firebird
firebird
Extension version => 7.0.0
```

---

**Document Maintenance:**
- Review quarterly or with major releases
- Update based on community feedback
- Track implementation progress in GitHub Issues

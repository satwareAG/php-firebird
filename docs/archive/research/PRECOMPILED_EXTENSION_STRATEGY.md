# Precompiled PHP Extension Distribution Strategy for php-firebird

## Executive Summary

This document outlines the strategy for distributing precompiled `firebird.so` PHP extensions with bundled Firebird client libraries using **rpath + $ORIGIN** approach. The goal is to provide drop-in installation packages that work across Linux distributions without requiring users to install Firebird client libraries system-wide.

**Target Version**: v7.0.0
**Decision**: Hybrid distribution (PECL source + GitHub Releases precompiled + Docker images)

## 1. Distribution Strategy Overview

### 1.1 Three-Tier Distribution Model

| Tier | Method | Target Audience | Complexity |
|------|--------|-----------------|------------|
| **Source** | PECL / GitHub | Advanced users, custom builds | High |
| **Precompiled** | GitHub Releases | Standard Linux users | Low |
| **Containerized** | Docker Hub | DevOps / Cloud deployments | None |

### 1.2 Build Matrix

**PHP Versions:**
| PHP | ZEND_MODULE_API_NO | Status | Support Timeline |
|-----|-------------------|--------|------------------|
| 8.1 | 20210902 | ⚠️ **DEPRECATED** | Drop in v7.1.0+ |
| 8.2 | 20220829 | Active | Full support |
| 8.3 | 20230831 | Active | Full support |
| 8.4 | 20240924 | Active | Full support |
| 8.5 | TBD (Nov 2025) | Current | Full support |

**Variants per PHP Version:**
- NTS (Non-Thread Safe) - Default
- ZTS (Zend Thread Safe) - For threaded SAPIs

**Architectures:**
- x86_64 (amd64) - Primary
- aarch64 (arm64) - Secondary (v7.1.0+)

**Total Builds**: 5 PHP versions × 2 variants × 1 arch = **10 packages per release**

### 1.3 Firebird Server Compatibility

| FB Server | Wire Protocol | Client Required | Status |
|-----------|---------------|-----------------|--------|
| 2.5 | v10-12 | FB 5.x (backward compat) | ⚠️ **DEPRECATED** |
| 3.0 | v13 | FB 5.x | Active |
| 4.0 | v15 | FB 5.x | Active |
| 5.0 | v16 | FB 5.x (native) | Current |

**Key Finding**: Firebird 5.x client is **backward compatible** with all Firebird server versions (2.5+) through wire protocol negotiation. We bundle only FB 5.x client.

---

## 2. manylinux_2_28 Build Environment

### 2.1 Platform Specification

| Property | Value |
|----------|-------|
| **Base Image** | `quay.io/pypa/manylinux_2_28_x86_64` |
| **OS Base** | AlmaLinux 8 |
| **glibc Version** | 2.28 |
| **GCC Toolset** | gcc-toolset-14 |
| **Compatibility** | Ubuntu 18.10+, Debian 10+, RHEL 8+, AlmaLinux 8+, Rocky 8+ |

### 2.2 Pre-installed Development Tools

```
autoconf, automake, bison, cmake, patchelf, curl, make, patch, unzip
glibc-devel, libstdc++-devel, zlib-devel, expat-devel
```

### 2.3 Why manylinux_2_28?

1. **2025 Standard**: Recommended baseline for new projects (PEP 600)
2. **Wide Compatibility**: Covers 99%+ of production Linux deployments
3. **Modern Toolchain**: GCC 14 with C++20/23 support
4. **Proven Infrastructure**: Battle-tested for Python wheel distribution
5. **Security**: Regular updates from AlmaLinux

### 2.4 Docker Usage

```bash
# Interactive build
docker run -it -v $(pwd):/src quay.io/pypa/manylinux_2_28_x86_64 /bin/bash

# Build command
docker run --rm -v $(pwd):/src quay.io/pypa/manylinux_2_28_x86_64 /src/scripts/build-precompiled.sh 8.4 nts x86_64
```

---

## 3. Library Bundling with rpath + $ORIGIN

### 3.1 The Problem

PHP extensions built against shared libraries (libfbclient.so) normally require:
1. Library installed system-wide (`/usr/lib`, `/usr/local/lib`)
2. `LD_LIBRARY_PATH` set at runtime
3. ldconfig cache updated

This creates dependency management overhead for users.

### 3.2 The Solution: $ORIGIN-relative rpath

ELF binaries can encode **relative** library search paths using the `$ORIGIN` token, which resolves to the directory containing the binary at runtime.

```
firebird.so (RPATH = $ORIGIN/lib)
└── lib/
    ├── libfbclient.so.5
    ├── libicuuc.so.72
    ├── libicudata.so.72
    └── ...
```

### 3.3 patchelf Implementation

**Tool**: `patchelf` (NixOS/patchelf) - ELF binary modifier

**Key Commands:**
```bash
# Set RPATH with $ORIGIN (MUST quote to prevent shell expansion)
patchelf --set-rpath '$ORIGIN/lib' firebird.so

# Use DT_RPATH instead of DT_RUNPATH (stronger precedence)
patchelf --force-rpath --set-rpath '$ORIGIN/lib' firebird.so

# Verify RPATH
patchelf --print-rpath firebird.so
# Output: $ORIGIN/lib

# Alternative verification
readelf -d firebird.so | grep -E 'RPATH|RUNPATH'
```

### 3.4 DT_RPATH vs DT_RUNPATH

| Attribute | DT_RPATH | DT_RUNPATH |
|-----------|----------|------------|
| **Precedence** | Before LD_LIBRARY_PATH | After LD_LIBRARY_PATH |
| **Scope** | Only this binary | This binary + dependencies |
| **Default** | No (legacy) | Yes (modern) |
| **Recommendation** | Use `--force-rpath` for self-contained bundles |

### 3.5 Dependency Chain Analysis

```bash
# List all dependencies
ldd firebird.so

# Example output:
#   linux-vdso.so.1 (system)
#   libfbclient.so.5 => not found  # <-- Must bundle
#   libpthread.so.0 => /lib64/...  # system, OK
#   libc.so.6 => /lib64/...        # system, OK

# List libfbclient dependencies
ldd /opt/firebird/lib/libfbclient.so.5
#   libicuuc.so.72 => not found    # <-- Must bundle
#   libicudata.so.72 => not found  # <-- Must bundle
#   libtommath.so.1 => not found   # <-- Must bundle
#   ...
```

### 3.6 Bundling Process

```bash
# 1. Create distribution structure
mkdir -p dist/lib

# 2. Copy extension
cp modules/firebird.so dist/

# 3. Bundle Firebird client
cp /opt/firebird/lib/libfbclient.so.5* dist/lib/

# 4. Bundle ICU (transitive dependency)
cp /usr/lib64/libicuuc.so.72* dist/lib/
cp /usr/lib64/libicudata.so.72* dist/lib/
cp /usr/lib64/libicui18n.so.72* dist/lib/

# 5. Bundle other transitive deps
cp /usr/lib64/libtommath.so.1* dist/lib/
cp /usr/lib64/libtomcrypt.so.1* dist/lib/

# 6. Patch main extension RPATH
patchelf --force-rpath --set-rpath '$ORIGIN/lib' dist/firebird.so

# 7. Patch transitive deps (libfbclient needs $ORIGIN)
patchelf --force-rpath --set-rpath '$ORIGIN' dist/lib/libfbclient.so.5

# 8. Verify
ldd dist/firebird.so  # Should show lib/libfbclient.so.5
```

---

## 4. Firebird Client Library Analysis

### 4.1 Library Dependencies

| Library | Purpose | Size (approx) | Bundled |
|---------|---------|---------------|---------|
| `libfbclient.so.5` | Firebird client API | ~5 MB | ✓ |
| `libicuuc.so.72` | ICU Unicode | ~2 MB | ✓ |
| `libicudata.so.72` | ICU data | ~30 MB | ✓ |
| `libicui18n.so.72` | ICU i18n | ~3 MB | ✓ |
| `libtommath.so.1` | Bignum math | ~200 KB | ✓ |
| `libtomcrypt.so.1` | Cryptography | ~500 KB | ✓ |
| `libz.so.1` | Compression | System | ✗ |
| `libpthread.so.0` | Threading | System | ✗ |
| `libc.so.6` | C library | System | ✗ |

**Total Bundle Size**: ~45 MB (compressed ~15 MB)

### 4.2 Licensing

| Component | License | Redistribution |
|-----------|---------|----------------|
| libfbclient | IDPL (MPL-1.1 variant) | ✓ OK with notice |
| ICU | Unicode License | ✓ OK |
| libtommath | Public Domain | ✓ OK |
| libtomcrypt | Public Domain | ✓ OK |

**Required**: Include LICENSE file with IDPL notice in bundle.

### 4.3 Wire Protocol Compatibility

Firebird uses automatic wire protocol negotiation:

```
Client (FB 5.x) ──connect──> Server (FB 2.5)
                <──negotiate── Protocol v12
                ──use v12──>   [Connected]
```

**Protocols by Version:**
- FB 2.5: v10, v11, v12
- FB 3.0: v13 (wire encryption, SRP auth)
- FB 4.0: v15 (batch operations)
- FB 5.0: v16 (profiler, parallel workers)

**Backward Compatibility Confirmed**: FB 5.x client supports v10-v16, connecting to any FB server version.

---

## 5. Bundle Structure

### 5.1 Package Layout

```
php-firebird-7.0.0-php84-nts-linux-x86_64/
├── firebird.so                 # PHP extension (RPATH=$ORIGIN/lib)
├── lib/
│   ├── libfbclient.so.5        # Firebird 5.x client (RPATH=$ORIGIN)
│   ├── libfbclient.so.5.0.2    # Actual library
│   ├── libicuuc.so.72          # ICU Unicode
│   ├── libicuuc.so.72.1        
│   ├── libicudata.so.72        # ICU Data
│   ├── libicudata.so.72.1
│   ├── libicui18n.so.72        # ICU i18n
│   ├── libicui18n.so.72.1
│   ├── libtommath.so.1         # BigNum
│   ├── libtommath.so.1.2.0
│   ├── libtomcrypt.so.1        # Crypto
│   └── libtomcrypt.so.1.18.2
├── LICENSE                     # IDPL + ICU licenses
├── README.md                   # Installation instructions
└── DEPRECATION.md              # PHP 8.1 / FB 2.5 notices
```

### 5.2 Installation

```bash
# 1. Extract to PHP extension directory
EXTDIR=$(php -r 'echo ini_get("extension_dir");')
tar -xzf php-firebird-7.0.0-php84-nts-linux-x86_64.tar.gz -C "$EXTDIR"

# 2. Enable extension
echo "extension=firebird.so" | sudo tee /etc/php/8.4/mods-available/firebird.ini
sudo phpenmod firebird

# 3. Verify
php -m | grep firebird
php -r "echo fbird_version();"
```

### 5.3 Alternative: Custom Location

```bash
# Extract to custom location
mkdir -p /opt/php-firebird
tar -xzf php-firebird-*.tar.gz -C /opt/php-firebird --strip-components=1

# Configure PHP (php.ini)
extension=/opt/php-firebird/firebird.so

# Works without LD_LIBRARY_PATH due to $ORIGIN rpath
```

---

## 6. Build Dockerfile

### 6.1 Dockerfile for manylinux_2_28

```dockerfile
# build/manylinux/Dockerfile
FROM quay.io/pypa/manylinux_2_28_x86_64

ARG FB_VERSION=5.0.2
ARG PHP_VERSION=8.4

LABEL maintainer="satware AG <support@satware.com>"
LABEL description="PHP Firebird extension build environment"

# Install Firebird 5.x client
RUN curl -fsSL "https://github.com/FirebirdSQL/firebird/releases/download/v${FB_VERSION}/Firebird-${FB_VERSION}.0-linux-x64.tar.gz" \
    -o /tmp/firebird.tar.gz && \
    tar -xzf /tmp/firebird.tar.gz -C /opt && \
    mv /opt/Firebird-* /opt/firebird && \
    rm /tmp/firebird.tar.gz

# Install multiple PHP versions
RUN for v in 8.1 8.2 8.3 8.4; do \
      yum install -y php${v//.}-devel php${v//.}-cli || true; \
    done

# Alternative: Build PHP from source with proper config
# (See scripts/build-php.sh for custom PHP builds)

ENV FB_ROOT=/opt/firebird
ENV PATH="/opt/firebird/bin:${PATH}"

WORKDIR /src
COPY scripts/build-precompiled.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/build-precompiled.sh

ENTRYPOINT ["build-precompiled.sh"]
CMD ["8.4", "nts", "x86_64"]
```

### 6.2 Custom PHP Build Script

For precise control, build PHP from source:

```bash
#!/bin/bash
# scripts/build-php.sh
PHP_VERSION="${1:-8.4.2}"

curl -fsSL "https://www.php.net/distributions/php-${PHP_VERSION}.tar.xz" | tar -xJ
cd "php-${PHP_VERSION}"
./configure \
    --prefix=/opt/php/${PHP_VERSION} \
    --enable-cli \
    --disable-cgi \
    --disable-phpdbg \
    --without-pear \
    --enable-shared
make -j$(nproc)
make install
```

---

## 7. Build Script

### 7.1 Complete Build Script

```bash
#!/bin/bash
# scripts/build-precompiled.sh
set -euo pipefail

# Arguments
PHP_VERSION="${1:-8.4}"
VARIANT="${2:-nts}"  # nts or zts
ARCH="${3:-x86_64}"

# Version info
EXT_VERSION="7.0.0"
FB_VERSION="5.0"

# Paths
FB_ROOT="${FB_ROOT:-/opt/firebird}"
DIST_NAME="php-firebird-${EXT_VERSION}-php${PHP_VERSION//.}-${VARIANT}-linux-${ARCH}"
DIST_DIR="dist/${DIST_NAME}"

echo "=== Building php-firebird ${EXT_VERSION} ==="
echo "PHP: ${PHP_VERSION} (${VARIANT})"
echo "Arch: ${ARCH}"
echo "Firebird: ${FB_VERSION}"

# Clean previous build
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}/lib"

# 1. Build extension
echo ">>> Building extension..."
phpize --clean 2>/dev/null || true
phpize
./configure --with-firebird="${FB_ROOT}"
make -j$(nproc)

# 2. Copy extension
echo ">>> Copying extension..."
cp modules/firebird.so "${DIST_DIR}/"

# 3. Bundle Firebird client
echo ">>> Bundling Firebird client..."
cp -P "${FB_ROOT}/lib/libfbclient.so"* "${DIST_DIR}/lib/"

# 4. Bundle ICU libraries
echo ">>> Bundling ICU libraries..."
for lib in libicuuc libicudata libicui18n; do
    # Find the library (could be in different locations)
    for path in /usr/lib64 /usr/lib /opt/firebird/lib; do
        if ls "${path}/${lib}.so"* 2>/dev/null; then
            cp -P "${path}/${lib}.so"* "${DIST_DIR}/lib/" 2>/dev/null || true
            break
        fi
    done
done

# 5. Bundle other dependencies
echo ">>> Bundling other dependencies..."
for lib in libtommath libtomcrypt libre2; do
    for path in /usr/lib64 /usr/lib /opt/firebird/lib; do
        if ls "${path}/${lib}.so"* 2>/dev/null; then
            cp -P "${path}/${lib}.so"* "${DIST_DIR}/lib/" 2>/dev/null || true
            break
        fi
    done
done

# 6. Patch RPATH for main extension
echo ">>> Patching RPATH..."
patchelf --force-rpath --set-rpath '$ORIGIN/lib' "${DIST_DIR}/firebird.so"

# 7. Patch RPATH for bundled libraries
for so in "${DIST_DIR}"/lib/*.so*; do
    if [[ -f "$so" && ! -L "$so" ]]; then
        # Libraries in lib/ need $ORIGIN to find each other
        patchelf --force-rpath --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
    fi
done

# 8. Create documentation
echo ">>> Creating documentation..."

cat > "${DIST_DIR}/LICENSE" << 'EOF'
PHP Firebird Extension
Copyright (c) satware AG and contributors

This extension is licensed under the PHP License v3.01.

Bundled Libraries:
- Firebird Client Library (IDPL - Initial Developer's Public License)
  https://firebirdsql.org/en/licensing/
- ICU - International Components for Unicode (Unicode License)
  https://unicode.org/copyright.html
- libtommath, libtomcrypt (Public Domain)
EOF

cat > "${DIST_DIR}/README.md" << EOF
# PHP Firebird Extension ${EXT_VERSION}

Pre-compiled PHP extension for Firebird database connectivity.

## Package Information

- **PHP Version**: ${PHP_VERSION} (${VARIANT})
- **Architecture**: ${ARCH}
- **Firebird Client**: ${FB_VERSION}.x (bundled)
- **glibc Requirement**: 2.28+ (Ubuntu 18.10+, Debian 10+, RHEL 8+)

## Installation

\`\`\`bash
# Find PHP extension directory
EXTDIR=\$(php -r 'echo ini_get("extension_dir");')

# Extract package
sudo tar -xzf ${DIST_NAME}.tar.gz -C "\$EXTDIR"

# Enable extension
echo "extension=firebird.so" | sudo tee /etc/php/${PHP_VERSION}/mods-available/firebird.ini
sudo phpenmod firebird

# Verify
php -m | grep firebird
\`\`\`

## Supported Firebird Servers

The bundled Firebird 5.x client is backward compatible with:
- Firebird 5.0 ✓
- Firebird 4.0 ✓
- Firebird 3.0 ✓
- Firebird 2.5 ✓ (deprecated)

## Documentation

- https://github.com/satwareAG/php-firebird
- https://www.firebirdsql.org/
EOF

cat > "${DIST_DIR}/DEPRECATION.md" << 'EOF'
# Deprecation Notices

## PHP 8.1 Support

⚠️ **PHP 8.1 support is DEPRECATED** in php-firebird 7.0.x and will be
**REMOVED** in version 7.1.0.

PHP 8.1 reaches end-of-life on November 25, 2025. Please upgrade to PHP 8.2+.

## Firebird 2.5 Server Support

⚠️ **Firebird 2.5 server support is DEPRECATED** and will be **REMOVED**
in version 7.1.0.

Firebird 2.5 reached end-of-life in September 2020. The bundled Firebird 5.x
client maintains backward compatibility, but this is no longer tested.

**Recommendation**: Migrate to Firebird 4.0+ for security updates and new features.
EOF

# 9. Verify bundle
echo ">>> Verifying bundle..."
echo "RPATH of firebird.so:"
patchelf --print-rpath "${DIST_DIR}/firebird.so"

echo ""
echo "Dependencies:"
cd "${DIST_DIR}" && ldd firebird.so && cd -

# 10. Create tarball
echo ">>> Creating tarball..."
tar -czvf "dist/${DIST_NAME}.tar.gz" -C dist "${DIST_NAME}"

echo ""
echo "=== Build complete ==="
echo "Output: dist/${DIST_NAME}.tar.gz"
ls -lh "dist/${DIST_NAME}.tar.gz"
```

---

## 8. GitHub Actions Integration

### 8.1 Release Workflow

```yaml
# .github/workflows/release-precompiled.yml
name: Build Precompiled Extensions

on:
  release:
    types: [created]
  workflow_dispatch:
    inputs:
      php_versions:
        description: 'PHP versions (comma-separated)'
        default: '8.1,8.2,8.3,8.4'

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        php: [8.1, 8.2, 8.3, 8.4]
        variant: [nts, zts]
    container:
      image: quay.io/pypa/manylinux_2_28_x86_64
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Install Firebird SDK
        run: |
          curl -fsSL "https://github.com/FirebirdSQL/firebird/releases/download/v5.0.2/Firebird-5.0.2.0-linux-x64.tar.gz" \
            -o /tmp/firebird.tar.gz
          tar -xzf /tmp/firebird.tar.gz -C /opt
          mv /opt/Firebird-* /opt/firebird
      
      - name: Install PHP ${{ matrix.php }}
        run: |
          # Install or build PHP ${{ matrix.php }} with ${{ matrix.variant }}
          ./scripts/install-php.sh ${{ matrix.php }} ${{ matrix.variant }}
      
      - name: Build Extension
        run: |
          ./scripts/build-precompiled.sh ${{ matrix.php }} ${{ matrix.variant }} x86_64
      
      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: php-firebird-php${{ matrix.php }}-${{ matrix.variant }}
          path: dist/*.tar.gz
      
      - name: Upload to Release
        if: github.event_name == 'release'
        uses: softprops/action-gh-release@v1
        with:
          files: dist/*.tar.gz
```

---

## 9. Testing & Verification

### 9.1 Bundle Verification Script

```bash
#!/bin/bash
# scripts/verify-bundle.sh
BUNDLE_DIR="${1:-.}"

echo "=== Bundle Verification ==="

# Check RPATH
echo "1. Checking RPATH..."
RPATH=$(patchelf --print-rpath "${BUNDLE_DIR}/firebird.so")
if [[ "$RPATH" == '$ORIGIN/lib' ]]; then
    echo "   ✓ RPATH correct: $RPATH"
else
    echo "   ✗ RPATH incorrect: $RPATH (expected \$ORIGIN/lib)"
    exit 1
fi

# Check dependencies resolve
echo "2. Checking dependencies..."
cd "${BUNDLE_DIR}"
MISSING=$(ldd firebird.so 2>&1 | grep "not found" || true)
if [[ -z "$MISSING" ]]; then
    echo "   ✓ All dependencies resolved"
else
    echo "   ✗ Missing dependencies:"
    echo "$MISSING"
    exit 1
fi

# Check PHP can load extension
echo "3. Testing PHP load..."
PHP_RESULT=$(php -d "extension=${PWD}/firebird.so" -m 2>&1 | grep -i firebird || true)
if [[ -n "$PHP_RESULT" ]]; then
    echo "   ✓ PHP loaded extension successfully"
else
    echo "   ✗ PHP failed to load extension"
    php -d "extension=${PWD}/firebird.so" -m 2>&1 | tail -5
    exit 1
fi

echo ""
echo "=== All checks passed ==="
```

### 9.2 Cross-Distribution Testing

Test on multiple distributions:

```bash
# Ubuntu 20.04 (glibc 2.31)
docker run --rm -v $(pwd)/dist:/dist ubuntu:20.04 \
    bash -c "apt update && apt install -y php-cli && /dist/verify-bundle.sh /dist/php-firebird-*"

# Debian 11 (glibc 2.31)  
docker run --rm -v $(pwd)/dist:/dist debian:11 \
    bash -c "apt update && apt install -y php-cli && /dist/verify-bundle.sh /dist/php-firebird-*"

# AlmaLinux 8 (glibc 2.28 - minimum)
docker run --rm -v $(pwd)/dist:/dist almalinux:8 \
    bash -c "dnf install -y php-cli && /dist/verify-bundle.sh /dist/php-firebird-*"
```

---

## 10. References

### 10.1 Official Documentation

- **manylinux**: https://github.com/pypa/manylinux
- **PEP 600**: https://peps.python.org/pep-0600/
- **patchelf**: https://github.com/NixOS/patchelf
- **auditwheel**: https://github.com/pypa/auditwheel
- **Firebird**: https://firebirdsql.org/en/documentation/

### 10.2 Related Research

- `docs/research/ADVANCED_DB_CLIENTS_2025.md` - Testing architecture
- `docs/research/PHP_EXTENSION_BUG_DETECTION_2025.md` - QA strategies

### 10.3 Key Findings Summary

| Topic | Finding |
|-------|---------|
| **Build Base** | manylinux_2_28 (AlmaLinux 8, glibc 2.28) |
| **Library Bundling** | patchelf + $ORIGIN rpath |
| **DT_RPATH** | Use `--force-rpath` for self-contained bundles |
| **FB Client** | 5.x is backward compatible with 2.5/3.0/4.0 servers |
| **Wire Protocol** | Automatic negotiation (v10-v16) |
| **Bundle Size** | ~45 MB uncompressed, ~15 MB compressed |

---

## Document History

| Date | Version | Author | Changes |
|------|---------|--------|---------|
| 2025-12-28 | 1.0 | satware AG / Cline | Initial research document |

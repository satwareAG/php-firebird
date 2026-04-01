#!/bin/bash
# =============================================================================
# PHP Firebird Extension - Precompiled Bundle Builder
# =============================================================================
#
# Build self-contained PHP extension packages with bundled Firebird client
# libraries using rpath + $ORIGIN for relocatable deployment.
#
# Usage:
#   ./scripts/build-precompiled.sh [options] [PHP_VERSION] [VARIANT] [ARCH]
#
# Arguments:
#   PHP_VERSION  - PHP version (8.1, 8.2, 8.3, 8.4, 8.5) [default: 8.4]
#   VARIANT      - nts (Non-Thread Safe) or zts (Thread Safe) [default: nts]
#   ARCH         - Architecture (x86_64, aarch64) [default: x86_64]
#
# Options:
#   --dry-run        Show what would be done without actually building
#   --skip-build     Skip extension compilation, use existing modules/firebird.so
#   --checksums      Generate SHA256 checksums file
#   --verbose        Show detailed output during bundling
#   --verify         Run verification after build
#   --help           Show this help message
#
# Environment:
#   FB_ROOT          Firebird installation directory [default: /opt/firebird]
#   EXT_VERSION      Extension version [default: from php_firebird.h]
#   PHP_PREFIX       PHP installation prefix [default: /opt/php]
#
# Output:
#   dist/php-firebird-{VERSION}-php{VER}-{VARIANT}-linux-{ARCH}.tar.gz
#   dist/php-firebird-{VERSION}-php{VER}-{VARIANT}-linux-{ARCH}.tar.gz.sha256
#
# =============================================================================

set -euo pipefail

# =============================================================================
# Configuration
# =============================================================================

PHP_VERSION=""
VARIANT=""
ARCH=""
PLATFORM=""
DRY_RUN=false
SKIP_BUILD=false
GENERATE_CHECKSUMS=true
VERBOSE=false
RUN_VERIFY=false

# Auto-detect platform
case "$(uname -s)" in
    Darwin) PLATFORM="macos" ;;
    *)      PLATFORM="linux" ;;
esac

# Auto-detect extension version
if [ -f "VERSION" ]; then
    DETECTED_VERSION=$(cat VERSION | tr -d '[:space:]')
elif [ -f "php_firebird.h" ]; then
    # Look for PHP_FIREBIRD_VERSION_STRING (set by configure)
    # Use || true to prevent grep exit code 1 from failing under set -e
    DETECTED_VERSION=$(grep -E '#define PHP_FIREBIRD_VERSION_STRING' php_firebird.h 2>/dev/null | sed 's/.*"\([^"]*\)".*/\1/' | head -1 || true)
    # If empty or contains "unknown", use fallback
    if [ -z "$DETECTED_VERSION" ] || [[ "$DETECTED_VERSION" == *"unknown"* ]]; then
        DETECTED_VERSION="7.0.0"
    fi
else
    DETECTED_VERSION="7.0.0"
fi
EXT_VERSION="${EXT_VERSION:-$DETECTED_VERSION}"

FB_VERSION="5.0"
FB_ROOT="${FB_ROOT:-/opt/firebird}"
PHP_PREFIX="${PHP_PREFIX:-/opt/php}"

# System libraries that should NOT be bundled
if [ "$PLATFORM" = "macos" ]; then
    SYSTEM_LIBS_WHITELIST=(
        "libSystem"
        "libc++.1.dylib"
        "libc++abi.dylib"
        "libobjc.A.dylib"
        "libz.1.dylib"
        "/usr/lib/lib"
        "/System/Library/"
    )
else
    SYSTEM_LIBS_WHITELIST=(
        "linux-vdso.so"
        "ld-linux"
        "ld-musl"
        "libc.so"
        "libpthread.so"
        "libdl.so"
        "libm.so"
        "librt.so"
        "libresolv.so"
        "libnsl.so"
        "libcrypt.so"
        "libgcc_s.so"
        "libstdc++.so"
    )
fi

# Library search paths
LIB_SEARCH_PATHS=(
    "${FB_ROOT}/lib"
    "/usr/lib64"
    "/usr/lib"
    "/usr/local/lib64"
    "/usr/local/lib"
    "/lib64"
    "/lib"
)

# Track bundled libraries to avoid duplicates
declare -A BUNDLED_LIBS

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# =============================================================================
# Helper Functions
# =============================================================================

usage() {
    sed -n '3,32p' "$0" | sed 's/^# //' | sed 's/^#//'
    exit 0
}

log_info() {
    echo -e "${GREEN}>>>${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}WARNING:${NC} $*"
}

log_error() {
    echo -e "${RED}ERROR:${NC} $*" >&2
}

log_verbose() {
    if [ "$VERBOSE" = true ]; then
        echo -e "${BLUE}[VERBOSE]${NC} $*"
    fi
}

log_dry_run() {
    echo -e "${BLUE}[DRY-RUN]${NC} $*"
}

check_command() {
    if ! command -v "$1" &>/dev/null; then
        log_error "Required command not found: $1"
        exit 1
    fi
}

# Check if library is a system library that shouldn't be bundled
is_system_lib() {
    local lib_name="$1"
    for pattern in "${SYSTEM_LIBS_WHITELIST[@]}"; do
        if [[ "$lib_name" == *"$pattern"* ]]; then
            return 0  # Is system lib
        fi
    done
    return 1  # Not system lib
}

# Find library in search paths
find_library() {
    local lib_name="$1"
    local ext_pattern="so"
    [ "$PLATFORM" = "macos" ] && ext_pattern="dylib"
    
    for search_path in "${LIB_SEARCH_PATHS[@]}"; do
        # Try exact name
        if [ -f "${search_path}/${lib_name}" ]; then
            echo "${search_path}/${lib_name}"
            return 0
        fi
        
        # Try with wildcard for version suffixes
        local base_name="${lib_name%.${ext_pattern}*}"
        base_name="${base_name%.so*}"  # Also strip .so if searching cross-platform
        for found in "${search_path}/${base_name}.${ext_pattern}"* "${search_path}/${base_name}.so"*; do
            if [ -f "$found" ]; then
                echo "$found"
                return 0
            fi
        done
    done
    
    return 1
}

# =============================================================================
# Bundle Library Function (with transitive dependency tracing)
# =============================================================================

bundle_library() {
    local lib_path="$1"
    local dest_dir="$2"
    local depth="${3:-0}"
    local indent=""
    
    # Indent for visual hierarchy
    for ((i=0; i<depth; i++)); do indent+="  "; done
    
    # Resolve symlinks to get real path
    local real_path
    real_path=$(readlink -f "$lib_path" 2>/dev/null || echo "$lib_path")
    local lib_name
    lib_name=$(basename "$lib_path")
    local real_name
    real_name=$(basename "$real_path")
    
    # Skip if already bundled
    if [[ -n "${BUNDLED_LIBS[$real_name]:-}" ]]; then
        log_verbose "${indent}Skip (already bundled): $lib_name"
        return 0
    fi
    
    # Skip system libraries
    if is_system_lib "$lib_name"; then
        log_verbose "${indent}Skip (system lib): $lib_name"
        return 0
    fi
    
    # Check if file exists
    if [ ! -f "$real_path" ]; then
        log_warn "${indent}Library not found: $lib_path"
        return 1
    fi
    
    log_verbose "${indent}Bundling: $lib_name -> $real_name"
    
    if [ "$DRY_RUN" = true ]; then
        log_dry_run "${indent}Would copy: $real_path -> ${dest_dir}/"
    else
        # Copy the actual library file
        cp "$real_path" "${dest_dir}/${real_name}"
        
        # Create symlink if names differ
        if [ "$lib_name" != "$real_name" ]; then
            ln -sf "$real_name" "${dest_dir}/${lib_name}"
        fi
        
        if [ "$PLATFORM" = "macos" ]; then
            # macOS: Create install_name symlinks for dylibs
            local base="${lib_name%%.*}"
            if [ ! -e "${dest_dir}/${base}.dylib" ]; then
                ln -sf "$real_name" "${dest_dir}/${base}.dylib" 2>/dev/null || true
            fi
        else
            # CRITICAL: Create SONAME symlink (e.g., libfbclient.so.2 -> libfbclient.so.5.0.3)
            # The extension links against the SONAME, not the versioned filename
            local soname
            soname=$(objdump -p "$real_path" 2>/dev/null | grep -E '^\s+SONAME' | awk '{print $2}' || true)
            if [ -n "$soname" ] && [ "$soname" != "$real_name" ] && [ ! -e "${dest_dir}/${soname}" ]; then
                ln -sf "$real_name" "${dest_dir}/${soname}"
                log_verbose "${indent}  Created SONAME symlink: ${soname} -> ${real_name}"
            fi
            
            # Also create .so symlink if needed (without version suffix)
            local base="${lib_name%%.*}"
            if [ ! -e "${dest_dir}/${base}.so" ]; then
                ln -sf "$real_name" "${dest_dir}/${base}.so" 2>/dev/null || true
            fi
        fi
    fi
    
    # Mark as bundled
    BUNDLED_LIBS[$real_name]=1
    BUNDLED_LIBS[$lib_name]=1
    
    # Trace transitive dependencies (max depth 5)
    if [ "$depth" -lt 5 ]; then
        local deps
        if [ "$PLATFORM" = "macos" ]; then
            # macOS: otool -L lists linked libraries
            deps=$(otool -L "$real_path" 2>/dev/null | tail -n +2 | awk '{print $1}' | grep -v "^@" || true)
        else
            deps=$(ldd "$real_path" 2>/dev/null | grep "=> /" | awk '{print $3}' || true)
        fi
        
        for dep in $deps; do
            local dep_name
            dep_name=$(basename "$dep")
            
            # Skip system libs and already bundled
            if is_system_lib "$dep_name"; then
                continue
            fi
            
            if [[ -n "${BUNDLED_LIBS[$dep_name]:-}" ]]; then
                continue
            fi
            
            # Recursively bundle dependency
            bundle_library "$dep" "$dest_dir" $((depth + 1))
        done
    fi
    
    return 0
}

# =============================================================================
# Detect PHP Variant
# =============================================================================

detect_php_variant() {
    local php_bin="${1:-php}"
    
    if ! command -v "$php_bin" &>/dev/null; then
        echo "nts"
        return
    fi
    
    local zts
    zts=$("$php_bin" -r 'echo PHP_ZTS ? "zts" : "nts";' 2>/dev/null || echo "nts")
    echo "$zts"
}

# =============================================================================
# Generate Checksums
# =============================================================================

generate_checksums() {
    local file="$1"
    local checksum_file="${file}.sha256"
    
    if [ "$DRY_RUN" = true ]; then
        log_dry_run "Would generate: $checksum_file"
        return
    fi
    
    log_info "Generating checksums..."
    
    # Generate SHA256
    sha256sum "$file" > "$checksum_file"
    
    # Also generate MD5 for compatibility
    md5sum "$file" > "${file}.md5"
    
    log_info "  SHA256: $(cat "$checksum_file" | awk '{print $1}')"
}

# =============================================================================
# Argument Parsing
# =============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --checksums)
            GENERATE_CHECKSUMS=true
            shift
            ;;
        --no-checksums)
            GENERATE_CHECKSUMS=false
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --verify)
            RUN_VERIFY=true
            shift
            ;;
        --help|-h)
            usage
            ;;
        -*)
            log_error "Unknown option: $1"
            exit 1
            ;;
        *)
            # Positional arguments
            if [ -z "$PHP_VERSION" ]; then
                PHP_VERSION="$1"
            elif [ -z "$VARIANT" ]; then
                VARIANT="$1"
            elif [ -z "$ARCH" ]; then
                ARCH="$1"
            else
                log_error "Unexpected argument: $1"
                exit 1
            fi
            shift
            ;;
    esac
done

# Set defaults
PHP_VERSION="${PHP_VERSION:-8.4}"
VARIANT="${VARIANT:-nts}"
ARCH="${ARCH:-x86_64}"

# =============================================================================
# Distribution Naming
# =============================================================================

PHP_VER_SHORT="${PHP_VERSION//.}"
DIST_NAME="php-firebird-${EXT_VERSION}-php${PHP_VER_SHORT}-${VARIANT}-${PLATFORM}-${ARCH}"
DIST_DIR="dist/${DIST_NAME}"

# =============================================================================
# Pre-flight Checks
# =============================================================================

echo ""
log_info "=== PHP Firebird Extension Builder ==="
echo ""
log_info "Extension: ${EXT_VERSION}"
log_info "PHP: ${PHP_VERSION} (${VARIANT})"
log_info "Architecture: ${ARCH}"
log_info "Firebird: ${FB_VERSION}"
if [ "$DRY_RUN" = true ]; then
    log_info "Mode: DRY RUN (no changes will be made)"
fi
echo ""

# Check required tools
check_command make
check_command tar
if [ "$PLATFORM" = "macos" ]; then
    check_command install_name_tool
    check_command otool
else
    check_command patchelf
    check_command objdump
fi

if [ "$SKIP_BUILD" = false ]; then
    check_command phpize
fi

# Check Firebird SDK
if [ ! -d "${FB_ROOT}" ]; then
    log_error "Firebird not found at ${FB_ROOT}"
    log_error "Set FB_ROOT environment variable or install Firebird"
    exit 1
fi

if [ ! -f "${FB_ROOT}/include/ibase.h" ] && [ ! -f "${FB_ROOT}/include/firebird/Interface.h" ]; then
    log_error "Firebird headers not found in ${FB_ROOT}/include"
    exit 1
fi

# Check PHP version
if command -v php &>/dev/null; then
    PHP_ACTUAL=$(php -r 'echo PHP_MAJOR_VERSION . "." . PHP_MINOR_VERSION;' 2>/dev/null || echo "unknown")
    if [ "${PHP_ACTUAL}" != "${PHP_VERSION}" ]; then
        log_warn "Active PHP version (${PHP_ACTUAL}) differs from target (${PHP_VERSION})"
    fi
    
    # Check ZTS/NTS
    PHP_ZTS=$(detect_php_variant)
    if [ "${PHP_ZTS}" != "${VARIANT}" ]; then
        log_warn "Active PHP variant (${PHP_ZTS}) differs from target (${VARIANT})"
    fi
fi

# =============================================================================
# Build Extension
# =============================================================================

if [ "$SKIP_BUILD" = false ]; then
    if [ "$DRY_RUN" = true ]; then
        log_dry_run "Would clean previous build"
        log_dry_run "Would run: phpize"
        log_dry_run "Would run: ./configure --with-firebird=${FB_ROOT}"
        log_dry_run "Would run: make -j$(nproc)"
    else
        log_info "Cleaning previous build..."
        phpize --clean 2>/dev/null || true
        
        log_info "Running phpize..."
        phpize
        
        log_info "Configuring with LTO..."
        export CFLAGS="${CFLAGS:-} -flto=auto"
        export LDFLAGS="${LDFLAGS:-} -flto=auto"
        ./configure --with-firebird="${FB_ROOT}"
        
        log_info "Compiling..."
        make -j"$(nproc)"
    fi
fi

# Verify build
if [ "$DRY_RUN" = false ] && [ ! -f "modules/firebird.so" ]; then
    log_error "Build failed - modules/firebird.so not found"
    exit 1
fi

# =============================================================================
# Create Distribution Directory
# =============================================================================

if [ "$DRY_RUN" = true ]; then
    log_dry_run "Would create: ${DIST_DIR}/lib/"
else
    rm -rf "${DIST_DIR}"
    mkdir -p "${DIST_DIR}/lib"
fi

# =============================================================================
# Bundle Extension
# =============================================================================

log_info "Copying extension..."

if [ "$DRY_RUN" = true ]; then
    log_dry_run "Would copy: modules/firebird.so -> ${DIST_DIR}/"
else
    cp modules/firebird.so "${DIST_DIR}/"
fi

# =============================================================================
# Bundle Libraries with Transitive Dependencies
# =============================================================================

log_info "Bundling libraries with transitive dependencies..."

# Find and bundle libfbclient
FB_CLIENT=""
if [ "$PLATFORM" = "macos" ]; then
    FB_LIB_PATTERNS=("libfbclient.dylib" "libfbclient.dylib*")
else
    FB_LIB_PATTERNS=("libfbclient.so*")
fi
for path in "${LIB_SEARCH_PATHS[@]}"; do
    for pattern in "${FB_LIB_PATTERNS[@]}"; do
        for lib in "${path}"/${pattern}; do
            if [ -f "$lib" ]; then
                FB_CLIENT="$lib"
                break 3
            fi
        done
    done
done

if [ -z "$FB_CLIENT" ]; then
    log_error "libfbclient not found!"
    exit 1
fi

log_info "  Primary library: $FB_CLIENT"
bundle_library "$FB_CLIENT" "${DIST_DIR}/lib" 0

# Detect and bundle ICU libraries (required by libfbclient)
log_info "Detecting ICU version..."
ICU_VERSION=""
for path in "${LIB_SEARCH_PATHS[@]}"; do
    for icu in "${path}"/libicuuc.so.*; do
        if [ -f "$icu" ]; then
            ICU_VERSION=$(echo "$icu" | sed -n 's/.*libicuuc\.so\.\([0-9][0-9]*\).*/\1/p' | head -1)
            log_info "  Found ICU version: ${ICU_VERSION:-unknown}"
            bundle_library "$icu" "${DIST_DIR}/lib" 0
            break 2
        fi
    done
done

# Bundle related ICU libraries
for icu_lib in libicudata libicui18n libicuio; do
    for path in "${LIB_SEARCH_PATHS[@]}"; do
        for lib in "${path}/${icu_lib}.so"*; do
            if [ -f "$lib" ]; then
                bundle_library "$lib" "${DIST_DIR}/lib" 0
                break 2
            fi
        done
    done
done

# Bundle libtommath and libtomcrypt (used by Firebird for crypto)
log_info "Bundling crypto libraries..."
for lib in libtommath libtomcrypt; do
    for path in "${LIB_SEARCH_PATHS[@]}"; do
        for found in "${path}/${lib}.so"*; do
            if [ -f "$found" ]; then
                bundle_library "$found" "${DIST_DIR}/lib" 0
                break 2
            fi
        done
    done
done

# Bundle re2 (used by Firebird 4.0+)
log_info "Bundling regex library..."
for path in "${LIB_SEARCH_PATHS[@]}"; do
    for lib in "${path}"/libre2.so*; do
        if [ -f "$lib" ]; then
            bundle_library "$lib" "${DIST_DIR}/lib" 0
            break 2
        fi
    done
done

# =============================================================================
# Patch RPATH
# =============================================================================

log_info "Patching RPATH for main extension..."

if [ "$DRY_RUN" = true ]; then
    if [ "$PLATFORM" = "macos" ]; then
        log_dry_run "Would run: install_name_tool -add_rpath @loader_path/lib ${DIST_DIR}/firebird.so"
    else
        log_dry_run "Would run: patchelf --force-rpath --set-rpath '\$ORIGIN/lib' ${DIST_DIR}/firebird.so"
    fi
else
    if [ "$PLATFORM" = "macos" ]; then
        # macOS: add @loader_path/lib rpath and rewrite libfbclient reference
        install_name_tool -add_rpath @loader_path/lib "${DIST_DIR}/firebird.so" 2>/dev/null || true
        # Rewrite libfbclient dependency to use @rpath
        FBCLIENT_DEP=$(otool -L "${DIST_DIR}/firebird.so" 2>/dev/null | grep libfbclient | awk '{print $1}' | head -1 || true)
        if [ -n "$FBCLIENT_DEP" ]; then
            install_name_tool -change "$FBCLIENT_DEP" "@rpath/libfbclient.dylib" "${DIST_DIR}/firebird.so"
            log_verbose "  Rewrote libfbclient dep: $FBCLIENT_DEP -> @rpath/libfbclient.dylib"
        fi
    else
        patchelf --force-rpath --set-rpath '$ORIGIN/lib' "${DIST_DIR}/firebird.so"
    fi
fi

log_info "Patching RPATH for bundled libraries..."
if [ "$DRY_RUN" = false ]; then
    if [ "$PLATFORM" = "macos" ]; then
        for dylib in "${DIST_DIR}"/lib/*.dylib; do
            if [[ -f "$dylib" && ! -L "$dylib" ]]; then
                # Set install_name to @rpath-relative for relocatability
                local_name=$(basename "$dylib")
                install_name_tool -id "@rpath/${local_name}" "$dylib" 2>/dev/null || true
                install_name_tool -add_rpath @loader_path "$dylib" 2>/dev/null || true
                log_verbose "  Patched: ${local_name}"
            fi
        done
    else
        for so in "${DIST_DIR}"/lib/*.so*; do
            if [[ -f "$so" && ! -L "$so" ]]; then
                # Libraries in lib/ need $ORIGIN to find each other
                patchelf --force-rpath --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
                log_verbose "  Patched: $(basename "$so")"
            fi
        done
    fi
fi

# =============================================================================
# Create Documentation
# =============================================================================

log_info "Creating documentation..."

if [ "$DRY_RUN" = true ]; then
    log_dry_run "Would create: ${DIST_DIR}/LICENSE"
    log_dry_run "Would create: ${DIST_DIR}/README.md"  
    log_dry_run "Would create: ${DIST_DIR}/DEPRECATION.md"
else
    cat > "${DIST_DIR}/LICENSE" << 'EOF'
PHP Firebird Extension
======================

Copyright (c) 2024-2025 satware AG and contributors
Licensed under the PHP License v3.01

Bundled Libraries
-----------------

Firebird Client Library (libfbclient)
  Copyright (c) 2000-2025 Firebird Foundation
  Licensed under IDPL (Initial Developer's Public License)
  https://firebirdsql.org/en/licensing/

ICU - International Components for Unicode
  Copyright (c) 1995-2025 Unicode, Inc.
  Licensed under the Unicode License Agreement
  https://www.unicode.org/copyright.html

libtommath, libtomcrypt
  Public Domain / WTFPL
  https://www.libtom.net/

re2
  Copyright (c) 2009 The RE2 Authors
  Licensed under BSD-3-Clause License
EOF

    cat > "${DIST_DIR}/README.md" << EOF
# PHP Firebird Extension ${EXT_VERSION}

Pre-compiled PHP extension for Firebird database connectivity with bundled
client libraries. No system-wide Firebird installation required.

## Package Information

| Property | Value |
|----------|-------|
| **Extension Version** | ${EXT_VERSION} |
| **PHP Version** | ${PHP_VERSION} (${VARIANT}) |
| **Architecture** | ${ARCH} |
| **Firebird Client** | ${FB_VERSION}.x (bundled) |
| **glibc Requirement** | 2.28+ |

## Compatibility

### Linux Distributions (glibc 2.28+)
- Ubuntu 18.10+
- Debian 10 (Buster)+
- RHEL / CentOS / AlmaLinux / Rocky Linux 8+
- Fedora 29+
- openSUSE Leap 15.1+

### Firebird Servers
The bundled Firebird 5.x client is **backward compatible** with all Firebird
server versions via wire protocol negotiation:

| Server Version | Protocol | Status |
|----------------|----------|--------|
| Firebird 5.0 | 18-19 | ✅ Full support |
| Firebird 4.0 | 16-17 | ✅ Full support |
| Firebird 3.0 | 13-15 | ✅ Full support |
| Firebird 2.5 | 12 | ⚠️ Deprecated (EOL Sep 2020) |

**How it works**: The client sends its supported protocol versions (10-19).
The server selects the highest common version. This enables a single binary
to connect to any supported Firebird server version.

**Note**: Windows local XNET connections require exact version matching.
Use TCP/IP (localhost:3050) for cross-version local connections on Windows.

## Installation

### Option 1: System Extension Directory (Recommended)

\`\`\`bash
# Find PHP extension directory
EXTDIR=\$(php -r 'echo ini_get("extension_dir");')

# Extract package
sudo tar -xzf ${DIST_NAME}.tar.gz -C "\$EXTDIR" --strip-components=1

# Enable extension
echo "extension=firebird.so" | sudo tee /etc/php/${PHP_VERSION}/mods-available/firebird.ini
sudo phpenmod firebird  # Debian/Ubuntu
# -- or --
echo "extension=firebird.so" | sudo tee -a /etc/php.ini  # RHEL/CentOS

# Verify
php -m | grep firebird
\`\`\`

### Option 2: Custom Location

\`\`\`bash
# Extract to custom path
mkdir -p /opt/php-firebird
tar -xzf ${DIST_NAME}.tar.gz -C /opt/php-firebird --strip-components=1

# Add to php.ini
echo "extension=/opt/php-firebird/firebird.so" >> /path/to/php.ini

# Verify (no LD_LIBRARY_PATH needed!)
php -d "extension=/opt/php-firebird/firebird.so" -m | grep firebird
\`\`\`

## Quick Test

\`\`\`php
<?php
// Test extension loading
var_dump(extension_loaded('firebird'));

// Test connection (requires running Firebird server)
\$dsn = 'localhost:/path/to/database.fdb';
\$db = fbird_connect(\$dsn, 'SYSDBA', 'masterkey');
if (\$db) {
    echo "Connected successfully!\\n";
    fbird_close(\$db);
}
?>
\`\`\`

## Troubleshooting

### Missing dependencies
\`\`\`bash
ldd /path/to/firebird.so | grep "not found"
\`\`\`
All dependencies should be bundled. If any are missing, report an issue.

### Symbol version errors
Ensure your system has glibc 2.28 or newer:
\`\`\`bash
ldd --version
\`\`\`

### Connection issues
The bundled client supports all wire protocols. For Firebird 3.0+ servers,
ensure WireCrypt and AuthServer settings are compatible.

## Documentation

- GitHub: https://github.com/satwareAG/php-firebird
- Firebird: https://firebirdsql.org/

## License

PHP License v3.01. See LICENSE file for details.
EOF

    cat > "${DIST_DIR}/DEPRECATION.md" << 'EOF'
# Deprecation Notices

## PHP 8.1 Support

⚠️ **PHP 8.1 support is DEPRECATED** in php-firebird 7.0.x and will be
**REMOVED** in version 7.1.0.

**Reason**: PHP 8.1 reaches end-of-life on November 25, 2025.

**Action Required**: Upgrade to PHP 8.2 or newer before php-firebird 7.1.0.

## Firebird 2.5 Server Support  

⚠️ **Firebird 2.5 server support is DEPRECATED** and will be **REMOVED**
in version 7.1.0.

**Reason**: Firebird 2.5 reached end-of-life in September 2020. No security
updates are provided.

**Technical Note**: The bundled Firebird 5.x client maintains backward
compatibility with Firebird 2.5 servers via wire protocol negotiation, but
this configuration is no longer tested and may have issues with:
- Legacy authentication (Legacy_Auth required)
- Wire encryption negotiation (WireCrypt must be Enabled, not Required)
- Character set handling with older metadata

**Action Required**: Migrate to Firebird 4.0+ for security updates and
modern features like inline blob handling, batch operations, and INT128.

## Timeline

| Version | PHP 8.1 | FB 2.5 |
|---------|---------|--------|
| 7.0.x | ⚠️ Deprecated | ⚠️ Deprecated |
| 7.1.0+ | ❌ Removed | ❌ Removed |

---

## Technical: Single Client Library Strategy

Starting with php-firebird 7.0.0, all distribution bundles include **only**
the latest Firebird 5.x client library. This is safe because:

1. **Wire Protocol Negotiation**: The Firebird client and server negotiate
   the highest common protocol version during connection establishment.

2. **Protocol Versions Supported**:
   | Protocol | Firebird Version | Features |
   |----------|------------------|----------|
   | 10 | 1.0+ | Baseline (from InterBase 6.0) |
   | 11 | 2.1+ | Message batching |
   | 12 | 2.5+ | Async cancellation |
   | 13-15 | 3.0+ | Auth plugins, encryption, compression |
   | 16-17 | 4.0+ | Statement timeouts |
   | 18-19 | 5.0+ | Scrollable cursors, inline blobs |

3. **Backwards Compatibility**: The Firebird 5.x client supports all
   protocols (10-19). When connecting to an older server, it automatically
   negotiates down to the server's maximum supported protocol.

4. **Feature Availability**: Features requiring newer protocols gracefully
   degrade or are unavailable on older servers. Core database operations
   (connect, query, transactions, blobs) work on all supported versions.

For more details, see: docs/research/firebird-client-compatibility.md
EOF
fi

# =============================================================================
# Verify Bundle
# =============================================================================

log_info "Verifying bundle..."

if [ "$DRY_RUN" = false ]; then
    echo ""
    if [ "$PLATFORM" = "macos" ]; then
        echo "RPATH of firebird.so:"
        otool -l "${DIST_DIR}/firebird.so" 2>/dev/null | grep -A2 "LC_RPATH" || echo "  (no rpath)"

        echo ""
        echo "Dependencies:"
        otool -L "${DIST_DIR}/firebird.so" || true
    else
        echo "RPATH of firebird.so:"
        patchelf --print-rpath "${DIST_DIR}/firebird.so"

        echo ""
        echo "Dependencies:"
        (cd "${DIST_DIR}" && ldd firebird.so) || true

        # Check for missing dependencies
        MISSING=$(cd "${DIST_DIR}" && ldd firebird.so 2>&1 | grep "not found" || true)
        if [ -n "${MISSING}" ]; then
            log_warn "Some dependencies not found (may be OK if they're glibc system libs):"
            echo "${MISSING}"
        fi
    fi

    echo ""
    echo "Bundle contents:"
    ls -la "${DIST_DIR}/"
    echo ""
    echo "Bundled libraries (${#BUNDLED_LIBS[@]} files):"
    ls -la "${DIST_DIR}/lib/" 2>/dev/null || echo "  (none)"
    
    # Size summary
    echo ""
    TOTAL_SIZE=$(du -sh "${DIST_DIR}" | cut -f1)
    log_info "Bundle size: ${TOTAL_SIZE}"
fi

# =============================================================================
# Create Tarball
# =============================================================================

log_info "Creating tarball..."

if [ "$DRY_RUN" = true ]; then
    log_dry_run "Would create: dist/${DIST_NAME}.tar.gz"
else
    tar -czvf "dist/${DIST_NAME}.tar.gz" -C dist "${DIST_NAME}"
fi

# =============================================================================
# Generate Checksums
# =============================================================================

if [ "$GENERATE_CHECKSUMS" = true ]; then
    if [ "$DRY_RUN" = false ]; then
        generate_checksums "dist/${DIST_NAME}.tar.gz"
    else
        log_dry_run "Would generate checksums"
    fi
fi

# =============================================================================
# Run Verification (Optional)
# =============================================================================

if [ "$RUN_VERIFY" = true ] && [ "$DRY_RUN" = false ]; then
    log_info "Running bundle verification..."
    if [ -x "scripts/verify-bundle.sh" ]; then
        ./scripts/verify-bundle.sh "${DIST_DIR}" --verbose
    else
        log_warn "Verification script not found: scripts/verify-bundle.sh"
    fi
fi

# =============================================================================
# Summary
# =============================================================================

echo ""
log_info "=== Build Complete ==="
echo ""

if [ "$DRY_RUN" = true ]; then
    log_dry_run "Would output: dist/${DIST_NAME}.tar.gz"
else
    echo "Output: dist/${DIST_NAME}.tar.gz"
    ls -lh "dist/${DIST_NAME}.tar.gz"
    
    if [ "$GENERATE_CHECKSUMS" = true ]; then
        echo ""
        echo "Checksums:"
        cat "dist/${DIST_NAME}.tar.gz.sha256"
    fi
    
    echo ""
    echo "To test:"
    echo "  cd ${DIST_DIR}"
    echo "  php -d 'extension=\$(pwd)/firebird.so' -m | grep firebird"
fi
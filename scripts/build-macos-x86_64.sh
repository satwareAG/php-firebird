#!/usr/bin/env bash
# =============================================================================
# Build macOS x86_64 bundles for php-firebird v10.6.1
# =============================================================================
#
# Self-contained script for Intel Mac (x86_64, macOS 10.15+).
# Mirrors .github/workflows/release-macos.yml exactly for the x86_64 arch.
#
# Usage:
#   bash ~/build-macos-x86_64.sh [options]
#
# Options:
#   --release-tag TAG     GitHub release tag to upload to  [default: v10.6.1]
#   --php-versions LIST   Comma-separated PHP minor versions [default: 8.2,8.3,8.4,8.5]
#   --skip-upload         Build bundles but do not upload to GitHub release
#   --skip-php-build      Skip PHP source builds if /opt/php/<ver>-<variant> already exists
#   --work-dir DIR        Working directory for repo clone  [default: ~/php-firebird]
#   --help                Show this help
#
# Environment:
#   GITHUB_TOKEN          GitHub token for release upload (falls back to gh auth)
#
# Requirements:
#   - macOS x86_64 (Intel Mac)
#   - Homebrew installed (/usr/local/bin/brew)
#   - gh CLI installed and authenticated (or GITHUB_TOKEN set)
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
RELEASE_TAG="v10.6.1"
PHP_VERSIONS="8.2,8.3,8.4,8.5"
SKIP_UPLOAD=false
SKIP_PHP_BUILD=false
WORK_DIR="${HOME}/php-firebird"
REPO="satwareAG/php-firebird"

FB_VERSION="5.0.3"
FB_BUILD="1683"
FB_ARCH="x64"                 # Firebird pkg suffix for Intel
BUNDLE_ARCH="x86_64"          # Bundle name suffix

NCPU=$(sysctl -n hw.ncpu 2>/dev/null || echo "4")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
log()  { echo ">>> $*"; }
warn() { echo "WARN: $*" >&2; }
die()  { echo "ERROR: $*" >&2; exit 1; }

usage() {
    sed -n '3,25p' "$0" | sed 's/^# //' | sed 's/^#//'
    exit 0
}

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case $1 in
        --release-tag)    RELEASE_TAG="$2";    shift 2 ;;
        --php-versions)   PHP_VERSIONS="$2";   shift 2 ;;
        --skip-upload)    SKIP_UPLOAD=true;    shift   ;;
        --skip-php-build) SKIP_PHP_BUILD=true; shift   ;;
        --work-dir)       WORK_DIR="$2";       shift 2 ;;
        --help|-h)        usage                        ;;
        *)                die "Unknown option: $1"     ;;
    esac
done

# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------
log "=== Preflight checks ==="

# Must be x86_64
MACHINE=$(uname -m)
if [ "$MACHINE" != "x86_64" ]; then
    die "This script must run on x86_64. Detected: $MACHINE"
fi
log "Architecture: x86_64 OK"

# Must be macOS
if [ "$(uname -s)" != "Darwin" ]; then
    die "This script must run on macOS"
fi
log "OS: macOS $(sw_vers -productVersion)"

# Homebrew
if ! command -v brew >/dev/null 2>&1; then
    die "Homebrew not found. Install from https://brew.sh first."
fi
log "Homebrew: $(brew --version | head -1)"

# gh CLI
if ! command -v gh >/dev/null 2>&1; then
    die "gh CLI not found. Install: brew install gh"
fi

if [ "$SKIP_UPLOAD" = false ]; then
    if [ -n "${GITHUB_TOKEN:-}" ]; then
        log "GitHub auth: GITHUB_TOKEN env var set"
        export GH_TOKEN="${GITHUB_TOKEN}"
    else
        if ! gh auth status >/dev/null 2>&1; then
            die "gh is not authenticated. Run: gh auth login OR set GITHUB_TOKEN env var"
        fi
        log "GitHub auth: gh CLI authenticated"
    fi
fi

# ---------------------------------------------------------------------------
# Install build dependencies
# ---------------------------------------------------------------------------
log ""
log "=== Installing build dependencies via Homebrew ==="
brew install autoconf automake libtool re2c bison pkg-config libiconv icu4c libxml2 2>/dev/null || true

# Bison: brew's version must take priority (system bison is too old for PHP)
BISON_BIN="$(brew --prefix bison)/bin"
export PATH="${BISON_BIN}:${PATH}"
log "bison: $(bison --version | head -1)"

# icu4c is keg-only; add to PKG_CONFIG_PATH
ICU_PREFIX="$(brew --prefix icu4c 2>/dev/null || echo "")"
if [ -n "${ICU_PREFIX}" ] && [ -d "${ICU_PREFIX}/lib/pkgconfig" ]; then
    export PKG_CONFIG_PATH="${ICU_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    log "PKG_CONFIG_PATH: $PKG_CONFIG_PATH"
fi

# libxml2 is keg-only; add to PKG_CONFIG_PATH to override macOS SDK headers
# This fixes PHP 8.4+ DOM ext compile error: "called object type 'bool' is not a function"
LIBXML2_PREFIX="$(brew --prefix libxml2 2>/dev/null || echo "")"
if [ -n "${LIBXML2_PREFIX}" ] && [ -d "${LIBXML2_PREFIX}/lib/pkgconfig" ]; then
    export PKG_CONFIG_PATH="${LIBXML2_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
    log "Added libxml2 (keg-only) to PKG_CONFIG_PATH"
fi

# ---------------------------------------------------------------------------
# Clone / update repo
# ---------------------------------------------------------------------------
log ""
log "=== Setting up repository at $WORK_DIR ==="
if [ -d "$WORK_DIR/.git" ]; then
    log "Repo exists - pulling latest..."
    git -C "$WORK_DIR" fetch --all
    git -C "$WORK_DIR" checkout satware-main
    git -C "$WORK_DIR" pull --ff-only
else
    log "Cloning repo..."
    mkdir -p "$(dirname "$WORK_DIR")"
    git clone "https://github.com/${REPO}.git" "$WORK_DIR"
    git -C "$WORK_DIR" checkout satware-main
fi

# All build operations run from the repo root
cd "$WORK_DIR"

# ---------------------------------------------------------------------------
# Read extension version
# ---------------------------------------------------------------------------
if [ -f VERSION.txt ]; then
    EXT_VERSION=$(cat VERSION.txt | tr -d '[:space:]')
else
    EXT_VERSION=$(grep -E '#define PHP_FIREBIRD_VERSION_STRING' php_firebird.h \
        | sed 's/.*"\([^"]*\)".*/\1/' | head -1 || echo "")
    if [ -z "$EXT_VERSION" ] || [[ "$EXT_VERSION" == *"unknown"* ]]; then
        EXT_VERSION="10.6.1"
    fi
fi
log "Extension version: $EXT_VERSION"

# Strip 'v' prefix from release tag for consistency
RELEASE_TAG_BARE="${RELEASE_TAG#v}"
if [ "$EXT_VERSION" != "$RELEASE_TAG_BARE" ]; then
    warn "VERSION file says '$EXT_VERSION' but release tag is '$RELEASE_TAG'"
fi

# ---------------------------------------------------------------------------
# Extract Firebird x86_64 SDK
# ---------------------------------------------------------------------------
log ""
log "=== Extracting Firebird ${FB_VERSION} SDK (${FB_ARCH}) ==="

sudo mkdir -p /opt/firebird
sudo chown "$(whoami)" /opt/firebird

# Re-use if already present and libfbclient exists
if [ -f /opt/firebird/lib/libfbclient.dylib ] && \
   [ -f /opt/firebird/include/ibase.h ]; then
    log "Firebird SDK already present at /opt/firebird - skipping extraction"
    FB_ARCH_ACTUAL=$(lipo -info /opt/firebird/lib/libfbclient.dylib 2>/dev/null | grep -o 'x86_64\|arm64' | head -1 || echo "unknown")
    if [ "$FB_ARCH_ACTUAL" != "x86_64" ]; then
        warn "Existing /opt/firebird may be wrong arch ($FB_ARCH_ACTUAL). Re-extracting..."
        rm -rf /opt/firebird
        sudo mkdir -p /opt/firebird
        sudo chown "$(whoami)" /opt/firebird
        bash ".github/scripts/extract-firebird-macos.sh" \
            --arch "${FB_ARCH}" \
            --fb-version "${FB_VERSION}" \
            --fb-build "${FB_BUILD}" \
            --output /opt/firebird
    fi
else
    chmod +x .github/scripts/extract-firebird-macos.sh
    bash ".github/scripts/extract-firebird-macos.sh" \
        --arch "${FB_ARCH}" \
        --fb-version "${FB_VERSION}" \
        --fb-build "${FB_BUILD}" \
        --output /opt/firebird
fi

# Verify x86_64
FB_DYLIB_ARCH=$(lipo -info /opt/firebird/lib/libfbclient.dylib 2>/dev/null | grep -o 'x86_64\|arm64' | head -1 || echo "")
if [ -n "$FB_DYLIB_ARCH" ] && [ "$FB_DYLIB_ARCH" != "x86_64" ]; then
    die "Firebird SDK at /opt/firebird is $FB_DYLIB_ARCH, not x86_64. Cannot continue."
fi
log "Firebird SDK arch: ${FB_DYLIB_ARCH:-unknown (file command may not know)}"

FB_ROOT=/opt/firebird

# ---------------------------------------------------------------------------
# PHP version resolution helper
# ---------------------------------------------------------------------------
resolve_php_version() {
    local MINOR="$1"
    local LATEST
    LATEST=$(curl -fsSL --retry 3 --retry-delay 5 \
        "https://www.php.net/releases/index.php?json&version=${MINOR}" \
        2>/dev/null | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('${MINOR}',{}).get('version',''))" \
        2>/dev/null || true)

    if [ -n "${LATEST}" ] && [[ "${LATEST}" =~ ^${MINOR}\. ]]; then
        echo "${LATEST}"
    else
        case "${MINOR}" in
            8.2) echo "8.2.28" ;;
            8.3) echo "8.3.16" ;;
            8.4) echo "8.4.4"  ;;
            8.5) echo "8.5.0"  ;;
            *)   echo "${MINOR}.0" ;;
        esac
    fi
}

# ---------------------------------------------------------------------------
# Prepare dist output directory
# ---------------------------------------------------------------------------
mkdir -p dist

BUILT_BUNDLES=()

# ---------------------------------------------------------------------------
# Main build loop: PHP versions × variants
# ---------------------------------------------------------------------------
IFS=',' read -ra PHP_VER_LIST <<< "${PHP_VERSIONS}"

for PHP_MINOR in "${PHP_VER_LIST[@]}"; do
    PHP_MINOR=$(echo "$PHP_MINOR" | tr -d ' ')

    log ""
    log "========================================================"
    log "Resolving PHP ${PHP_MINOR} full version..."
    FULL_VERSION=$(resolve_php_version "$PHP_MINOR")
    log "PHP ${PHP_MINOR} -> ${FULL_VERSION}"

    PHP_VER_SHORT="${PHP_MINOR//.}"   # "8.2" -> "82"

    for VARIANT in nts zts; do
        INSTALL_DIR="/opt/php/${PHP_MINOR}-${VARIANT}"

        log ""
        log "-------- PHP ${FULL_VERSION} (${VARIANT}) --------"

        # ---------------------------------------------------------------
        # Build PHP from source
        # ---------------------------------------------------------------
        if [ "$SKIP_PHP_BUILD" = true ] && [ -x "${INSTALL_DIR}/bin/php" ]; then
            log "PHP ${PHP_MINOR}-${VARIANT} already built at $INSTALL_DIR - skipping"
        else
            log "Building PHP ${FULL_VERSION} (${VARIANT})..."

            sudo mkdir -p /opt/php
            sudo chown "$(whoami)" /opt/php

            PHP_TARBALL="/tmp/php-${FULL_VERSION}.tar.xz"
            if [ ! -f "$PHP_TARBALL" ]; then
                curl -fsSL \
                    "https://www.php.net/distributions/php-${FULL_VERSION}.tar.xz" \
                    -o "$PHP_TARBALL"
            fi

            PHP_SRC="/tmp/php-src-${PHP_MINOR}-${VARIANT}"
            rm -rf "$PHP_SRC"
            mkdir -p "$PHP_SRC"
            tar -xJf "$PHP_TARBALL" -C "$PHP_SRC" --strip-components=1

            cd "$PHP_SRC"

            ICONV_PREFIX="$(brew --prefix libiconv 2>/dev/null || echo "")"

            CONFIGURE_OPTS=(
                "--prefix=${INSTALL_DIR}"
                "--enable-cli"
                "--disable-cgi"
                "--disable-phpdbg"
                "--without-pear"
                "--enable-shared"
                "--enable-mbstring"
                "--enable-intl"
                "--enable-opcache"
                "--with-openssl"
                "--with-zlib"
                "--with-curl"
            )

            if [ -n "${ICONV_PREFIX}" ] && [ -d "${ICONV_PREFIX}" ]; then
                CONFIGURE_OPTS+=("--with-iconv=${ICONV_PREFIX}")
            fi

            # Use Homebrew's libxml2 to avoid macOS SDK header conflicts
            # (SDK globals.h: #define xmlFree(ptr) free(ptr) breaks PHP 8.4 DOM)
            LIBXML2_OPT="$(brew --prefix libxml2 2>/dev/null || echo "")"
            if [ -n "${LIBXML2_OPT}" ] && [ -d "${LIBXML2_OPT}" ]; then
                CONFIGURE_OPTS+=("--with-libxml-dir=${LIBXML2_OPT}")
            fi

            if [ "${VARIANT}" = "zts" ]; then
                CONFIGURE_OPTS+=("--enable-zts")
            fi

            ./configure "${CONFIGURE_OPTS[@]}"
            make -j"${NCPU}"
            make install

            "${INSTALL_DIR}/bin/php" -v

            cd "$WORK_DIR"
            rm -rf "$PHP_SRC"
            # Keep tarball cached for other variants of same version
        fi

        # ---------------------------------------------------------------
        # Build the extension
        # ---------------------------------------------------------------
        log "Building firebird extension (PHP ${PHP_MINOR} ${VARIANT})..."

        export PATH="${INSTALL_DIR}/bin:${PATH}"
        export FB_ROOT=/opt/firebird

        # Clean previous build state
        phpize --clean 2>/dev/null || true

        export CFLAGS="${CFLAGS:-} -O2"
        export LDFLAGS="${LDFLAGS:-} -L${FB_ROOT}/lib"
        phpize
        ./configure --with-firebird="${FB_ROOT}"

        make -j"${NCPU}"

        log "Extension built:"
        ls -la modules/firebird.so
        file modules/firebird.so

        # ---------------------------------------------------------------
        # Create bundle
        # ---------------------------------------------------------------
        BUNDLE_NAME="php-firebird-${EXT_VERSION}-php${PHP_VER_SHORT}-${VARIANT}-macos-${BUNDLE_ARCH}"
        DIST_DIR="dist/${BUNDLE_NAME}"

        log "Creating bundle: ${BUNDLE_NAME}"

        rm -rf "${DIST_DIR}"
        mkdir -p "${DIST_DIR}/lib"

        # Copy extension
        cp modules/firebird.so "${DIST_DIR}/firebird.so"

        # Bundle libfbclient (real files only, no symlinks)
        for dylib in "${FB_ROOT}"/lib/libfbclient*.dylib; do
            if [ -f "$dylib" ] && [ ! -L "$dylib" ]; then
                cp "$dylib" "${DIST_DIR}/lib/"
                log "Bundled: $(basename "$dylib")"
            fi
        done

        # Create symlinks for versioned dylibs inside lib/
        pushd "${DIST_DIR}/lib" >/dev/null
        for lib in libfbclient.dylib.*; do
            if [ -f "$lib" ] && [ ! -L "$lib" ]; then
                [ ! -e "libfbclient.dylib" ] && ln -sf "$lib" "libfbclient.dylib" 2>/dev/null || true
            fi
        done 2>/dev/null || true
        popd >/dev/null

        # RPATH patching
        log "Patching RPATH..."
        install_name_tool -add_rpath @loader_path/lib "${DIST_DIR}/firebird.so" 2>/dev/null || true

        FB_DYLIB_ID=$(otool -L "${DIST_DIR}/firebird.so" \
            | grep -o '[^ ]*libfbclient[^ ]*' | head -1 || true)
        if [ -n "${FB_DYLIB_ID}" ]; then
            log "Rewriting: ${FB_DYLIB_ID} -> @rpath/libfbclient.dylib"
            install_name_tool -change "${FB_DYLIB_ID}" \
                "@rpath/libfbclient.dylib" "${DIST_DIR}/firebird.so"
        fi

        if [ -f "${DIST_DIR}/lib/libfbclient.dylib" ]; then
            install_name_tool -id "@rpath/libfbclient.dylib" \
                "${DIST_DIR}/lib/libfbclient.dylib" 2>/dev/null || true
        fi

        # Verify RPATH
        log "RPATH verification:"
        otool -l "${DIST_DIR}/firebird.so" | grep -A2 LC_RPATH || echo "(no LC_RPATH)"
        otool -L "${DIST_DIR}/firebird.so"

        # Documentation files
        cat > "${DIST_DIR}/LICENSE" <<'LICEOF'
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
LICEOF

        cat > "${DIST_DIR}/README.md" <<READMEOF
# PHP Firebird Extension ${EXT_VERSION} - macOS x86_64

Pre-compiled PHP extension for Firebird database connectivity with bundled
client libraries. No system-wide Firebird installation required.

## Package Information

| Property | Value |
|----------|-------|
| **Extension Version** | ${EXT_VERSION} |
| **PHP Version** | ${PHP_MINOR} (${VARIANT}) |
| **Architecture** | x86_64 |
| **Platform** | macOS |

## Installation

\`\`\`bash
EXTDIR=\$(php -r 'echo ini_get("extension_dir");')
sudo tar -xzf ${BUNDLE_NAME}.tar.gz -C "\$EXTDIR" --strip-components=1
echo "extension=firebird.so" | sudo tee /usr/local/etc/php/${PHP_MINOR}/conf.d/firebird.ini
php -m | grep firebird
\`\`\`
READMEOF

        # Create tarball
        log "Creating tarball..."
        tar -czvf "dist/${BUNDLE_NAME}.tar.gz" -C dist "${BUNDLE_NAME}"

        # Checksums
        pushd dist >/dev/null
        shasum -a 256 "${BUNDLE_NAME}.tar.gz" | awk '{print $1}' > "${BUNDLE_NAME}.tar.gz.sha256"
        md5 -q "${BUNDLE_NAME}.tar.gz" > "${BUNDLE_NAME}.tar.gz.md5" 2>/dev/null || \
            md5sum "${BUNDLE_NAME}.tar.gz" | awk '{print $1}' > "${BUNDLE_NAME}.tar.gz.md5"
        popd >/dev/null

        # Optional SBOM via syft
        if command -v syft >/dev/null 2>&1; then
            log "Generating SBOM (syft)..."
            syft dir:"${DIST_DIR}" \
                --output cyclonedx-json \
                --file "dist/${BUNDLE_NAME}.sbom.cdx.json" 2>/dev/null || \
                warn "SBOM generation failed (non-fatal)"
        else
            log "syft not found - skipping SBOM (install with: brew install syft)"
        fi

        log "Bundle ready:"
        ls -lh "dist/${BUNDLE_NAME}.tar.gz" "dist/${BUNDLE_NAME}.tar.gz.sha256"

        BUILT_BUNDLES+=("${BUNDLE_NAME}")

        # Clean phpize/configure artifacts to avoid state leakage between variants
        make distclean 2>/dev/null || true

    done  # variant loop

    # Clean PHP tarball after both NTS and ZTS are built for this version
    rm -f "/tmp/php-${FULL_VERSION}.tar.xz"

done  # PHP version loop

# ---------------------------------------------------------------------------
# Summary of built bundles
# ---------------------------------------------------------------------------
log ""
log "=== Build complete ==="
log "Bundles built: ${#BUILT_BUNDLES[@]}"
for b in "${BUILT_BUNDLES[@]}"; do
    echo "  $b"
done

echo ""
echo "dist/ contents:"
ls -lh dist/*.tar.gz dist/*.sha256 2>/dev/null | sort

# ---------------------------------------------------------------------------
# Upload to GitHub release
# ---------------------------------------------------------------------------
if [ "$SKIP_UPLOAD" = true ]; then
    log ""
    log "=== Skipping upload (--skip-upload) ==="
    log "To upload manually:"
    for b in "${BUILT_BUNDLES[@]}"; do
        echo "  gh release upload ${RELEASE_TAG} dist/${b}.tar.gz dist/${b}.tar.gz.sha256 --repo ${REPO} --clobber"
    done
    exit 0
fi

log ""
log "=== Uploading bundles to ${RELEASE_TAG} ==="

UPLOAD_ERRORS=0

for b in "${BUILT_BUNDLES[@]}"; do
    TARBALL="dist/${b}.tar.gz"
    SHA256="dist/${b}.tar.gz.sha256"

    log "Uploading: ${b}..."

    UPLOAD_FILES=("$TARBALL" "$SHA256")

    # Include SBOM if it was generated
    if [ -f "dist/${b}.sbom.cdx.json" ]; then
        UPLOAD_FILES+=("dist/${b}.sbom.cdx.json")
    fi

    if gh release upload "${RELEASE_TAG}" \
        "${UPLOAD_FILES[@]}" \
        --repo "${REPO}" \
        --clobber; then
        log "Uploaded: ${b}"
    else
        warn "Upload FAILED for: ${b}"
        UPLOAD_ERRORS=$((UPLOAD_ERRORS + 1))
    fi
done

# ---------------------------------------------------------------------------
# Final verification
# ---------------------------------------------------------------------------
log ""
log "=== Verifying release assets ==="
gh release view "${RELEASE_TAG}" \
    --repo "${REPO}" \
    --json assets 2>/dev/null \
    | python3 -c "
import sys, json
assets = json.load(sys.stdin).get('assets', [])
x86 = sorted(a['name'] for a in assets if 'x86_64' in a['name'])
arm = sorted(a['name'] for a in assets if 'arm64' in a['name'])
print(f'x86_64 assets ({len(x86)}):')
for n in x86: print(f'  {n}')
print(f'arm64 assets ({len(arm)}):')
for n in arm: print(f'  {n}')
" || true

if [ "$UPLOAD_ERRORS" -gt 0 ]; then
    die "${UPLOAD_ERRORS} upload(s) failed. Check output above."
fi

log ""
log "=== All done! ==="
log "x86_64 bundles uploaded to ${RELEASE_TAG}"
log "Total bundles: ${#BUILT_BUNDLES[@]}"

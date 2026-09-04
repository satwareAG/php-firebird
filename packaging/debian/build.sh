#!/usr/bin/env bash
# =============================================================================
# packaging/debian/build.sh
# =============================================================================
#
# Builds one .deb package for php-firebird inside a Docker container.
#
# Usage:
#   packaging/debian/build.sh --php-version 8.4 --distro bookworm --arch x86_64
#
# This script is designed to run INSIDE a Docker container (e.g.,
# php:8.4-cli-bookworm) that has:
#   - PHP dev headers (php8.4-dev or built from source)
#   - Build tools (autoconf, gcc, g++, make, patchelf)
#   - Docker (for running the container)
#
# The script:
#   1. Fetches the FB5 client via fetch-client.sh (#486)
#   2. Runs phpize + configure + make to build firebird.so + pdo_fbird.so
#   3. Sets RPATH on the extension .so files
#   4. Runs dpkg-buildpackage to produce the .deb
#   5. Outputs the path to the .deb file
#
# =============================================================================

set -euo pipefail

# -----------------------------------------------------------------------------
# Defaults
# -----------------------------------------------------------------------------

PHP_VERSION="8.4"
DISTRO="bookworm"
ARCH="x86_64"
FB_ROOT="${FB_ROOT:-/opt/firebird}"
OUTPUT_DIR="${OUTPUT_DIR:-dist/deb}"
BUILD_DIR="${BUILD_DIR:-}"
VERBOSE=0

# -----------------------------------------------------------------------------
# Logging
# -----------------------------------------------------------------------------

log() { [ "$VERBOSE" -eq 1 ] && echo ">>> $*" >&2 || true; }
log_info() { echo "INFO: $*" >&2; }
log_error() { echo "ERROR: $*" >&2; }

# -----------------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------------

usage() {
    sed -n '3,40p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --php-version) PHP_VERSION="$2"; shift 2 ;;
        --php-version=*) PHP_VERSION="${1#--php-version=}"; shift ;;
        --distro) DISTRO="$2"; shift 2 ;;
        --distro=*) DISTRO="${1#--distro=}"; shift ;;
        --arch) ARCH="$2"; shift 2 ;;
        --arch=*) ARCH="${1#--arch=}"; shift ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --output-dir=*) OUTPUT_DIR="${1#--output-dir=}"; shift ;;
        --verbose|-v) VERBOSE=1; shift ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --build-dir=*) BUILD_DIR="${1#--build-dir=}"; shift ;;
        --help|-h) usage ;;
        *) echo "ERROR: Unknown argument: $1" >&2; exit 1 ;;
    esac
done

# Normalize architecture names
case "$ARCH" in
    x86_64|amd64) ARCH="x86_64"; DEB_ARCH="amd64" ;;
    aarch64|arm64) ARCH="aarch64"; DEB_ARCH="arm64" ;;
    armv7l|armhf) ARCH="armv7l"; DEB_ARCH="armhf" ;;
    *) echo "ERROR: Unsupported arch: $ARCH" >&2; exit 1 ;;
esac

PHP_VER_SHORT="${PHP_VERSION//./}"

log_info "Building php-firebird .deb for PHP ${PHP_VERSION} on ${DISTRO} (${DEB_ARCH})"

# -----------------------------------------------------------------------------
# Resolve phpize/php-config commands (try versioned, fall back to unversioned)
# -----------------------------------------------------------------------------

PHPIZE=$(command -v "phpize${PHP_VERSION}" 2>/dev/null || command -v phpize 2>/dev/null || echo "")
PHPCONFIG=$(command -v "php-config${PHP_VERSION}" 2>/dev/null || command -v php-config 2>/dev/null || echo "")

if [ -z "$PHPIZE" ] || [ -z "$PHPCONFIG" ]; then
    log_error "phpize or php-config not found for PHP ${PHP_VERSION}"
    log_error "  phpize: ${PHPIZE:-not found}"
    log_error "  php-config: ${PHPCONFIG:-not found}"
    exit 1
fi

log "Using: $PHPIZE, $PHPCONFIG"

# -----------------------------------------------------------------------------
# Resolve script directory (works inside and outside Docker)
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Out-of-tree build: if BUILD_DIR is set, copy source there and work from it.
# jane: REPO_ROOT is repointed to BUILD_DIR so all cd "$REPO_ROOT" calls and
# packaging/debian/* path references resolve to the build copy, not the
# read-only source. This prevents build artifacts from polluting the source
# tree and fixes .deb version mismatches (stale debian/changelog template).
if [ -n "$BUILD_DIR" ] && [ "$BUILD_DIR" != "$SOURCE_ROOT" ]; then
    log_info "Out-of-tree build: copying source to ${BUILD_DIR}"
    mkdir -p "$BUILD_DIR"
    rm -rf "${BUILD_DIR}"/* "${BUILD_DIR}"/.[!.]* 2>/dev/null || true
    cp -a "$SOURCE_ROOT"/* "$BUILD_DIR/"
    REPO_ROOT="$BUILD_DIR"
else
    REPO_ROOT="$SOURCE_ROOT"
fi
cd "$REPO_ROOT"

# -----------------------------------------------------------------------------
# Step 1: Fetch FB5 client
# -----------------------------------------------------------------------------

if [ ! -f "${FB_ROOT}/lib/libfbclient.so" ]; then
    log_info "Fetching FB5 client..."
    bash packaging/fb-client-bundle/fetch-client.sh \
        --arch "$ARCH" \
        --output-dir "$FB_ROOT" \
        --verbose
else
    log "FB5 client already present at ${FB_ROOT}"
fi

# Verify FB5 client
if [ ! -f "${FB_ROOT}/lib/libfbclient.so" ] || [ ! -f "${FB_ROOT}/include/ibase.h" ]; then
    log_error "FB5 client not properly installed at ${FB_ROOT}"
    exit 1
fi

# jane: FB5 tarball includes libfbclient + libtomcrypt but NOT libtommath.
#       Copy libtommath.so.1 from the system (apt: libtommath1) so it gets
#       bundled in the .deb package. libfbclient depends on libtommath at runtime.
#       Use cp -L (dereference symlinks) to copy the real file, not the symlink.
for libpath in \
    /usr/lib/x86_64-linux-gnu/libtommath.so* \
    /usr/lib/aarch64-linux-gnu/libtommath.so* \
    /usr/lib/libtommath.so* \
    /usr/lib64/libtommath.so*; do
    if ls $libpath >/dev/null 2>&1; then
        cp -L $libpath "${FB_ROOT}/lib/" 2>/dev/null || true
        # Also create the SONAME symlink if not present
        cd "${FB_ROOT}/lib" 2>/dev/null || true
        for f in libtommath.so.*.*; do
            [ -f "$f" ] || continue
            soname=$(echo "$f" | sed -E 's/^(libtommath\.so\.[0-9]+).*/\1/')
            [ ! -e "$soname" ] && ln -sf "$f" "$soname" 2>/dev/null || true
        done
        [ ! -e "libtommath.so" ] && ln -sf "libtommath.so.1" "libtommath.so" 2>/dev/null || true
        cd "$REPO_ROOT" 2>/dev/null || true
        log "Copied libtommath from system to ${FB_ROOT}/lib/"
        break
    fi
done

# jane: config.m4 uses $PHP_LIBDIR from PHP's configure options.
#       On Debian multiarch: lib/x86_64-linux-gnu (or lib/aarch64-linux-gnu on ARM)
#       fetch-client.sh installs to lib/, so we need to create the multiarch
#       subdirectory and symlink the .so files there.
#       Also handle lib64 for non-Debian distros.
MULTIARCH_LIBDIR=$($PHPCONFIG --configure-options 2>/dev/null | grep -oP 'with-libdir=\K[^ ]+' || echo "lib")
log "PHP_LIBDIR: $MULTIARCH_LIBDIR"
if [ "$MULTIARCH_LIBDIR" != "lib" ]; then
    # Create multiarch dir as subdirectory of lib/
    # e.g. /opt/firebird/lib/x86_64-linux-gnu/
    MULTIARCH_PATH="${FB_ROOT}/lib/${MULTIARCH_LIBDIR}"
    mkdir -p "$MULTIARCH_PATH" 2>/dev/null || true
    # Symlink all .so* files from parent lib/ dir
    cd "${FB_ROOT}/lib"
    for so in *.so*; do
        [ -e "$so" ] && ln -sf "../$so" "${MULTIARCH_PATH}/$so" 2>/dev/null || true
    done
    cd "$REPO_ROOT"
    log "Created multiarch symlink dir: $MULTIARCH_PATH"
fi
# Also handle lib64 (some distros use lib64 instead of lib)
rm -rf "${FB_ROOT}/lib64" 2>/dev/null || true
ln -sf lib "${FB_ROOT}/lib64" 2>/dev/null || true

# -----------------------------------------------------------------------------
# Step 2: Build the extension (phpize + configure + make)
# -----------------------------------------------------------------------------

log_info "Building firebird extension for PHP ${PHP_VERSION}..."

# Clean previous builds
find . -name '*.dep' -delete 2>/dev/null || true
find . -name '*.lo' -delete 2>/dev/null || true
rm -rf .libs modules/ pdo_fbird/.libs pdo_fbird/modules/ 2>/dev/null || true

if [ -f Makefile ]; then
    make clean 2>/dev/null || true
fi
"$PHPIZE" --clean 2>/dev/null || true
cd pdo_fbird && "$PHPIZE" --clean 2>/dev/null || true; cd "$REPO_ROOT"

rm -f configure config.h config.h.in config.log config.status config.nice \
     Makefile Makefile.fragments Makefile.global Makefile.objects \
     pdo_fbird/Makefile pdo_fbird/configure pdo_fbird/config.h* 2>/dev/null || true

# phpize
log "Running phpize..."
"$PHPIZE"

# configure
log "Running configure..."
export CFLAGS="-I${FB_ROOT}/include"
export LDFLAGS="-L${FB_ROOT}/lib -Wl,-rpath-link,${FB_ROOT}/lib"
./configure \
    --with-php-config="$PHPCONFIG" \
    --with-firebird="${FB_ROOT}"

# make
log "Running make..."
make -j"$(nproc)"

# Build pdo_fbird
log_info "Building pdo_fbird extension..."
cd pdo_fbird
"$PHPIZE"
./configure \
    --with-php-config="$PHPCONFIG" \
    --with-firebird="${FB_ROOT}"
make -j"$(nproc)"
cd "$REPO_ROOT"

# Verify builds
if [ ! -f modules/firebird.so ]; then
    log_error "firebird.so build failed"
    exit 1
fi
if [ ! -f pdo_fbird/modules/pdo_fbird.so ]; then
    log_error "pdo_fbird.so build failed"
    exit 1
fi

log_info "Extension built successfully"

# -----------------------------------------------------------------------------
# Step 3: Set RPATH on extension .so files
# -----------------------------------------------------------------------------

# Get the PHP extension directory
EXT_DIR=$("$PHPCONFIG" --extension-dir)
log "PHP extension dir: ${EXT_DIR}"

# Calculate RPATH: $ORIGIN/../../php-firebird
# EXT_DIR is like /usr/lib/php/20240924
# $ORIGIN = /usr/lib/php/20240924
# $ORIGIN/../../php-firebird = /usr/lib/php-firebird
log "Setting RPATH on firebird.so..."
patchelf --force-rpath --set-rpath '$ORIGIN/../../php-firebird' modules/firebird.so
patchelf --force-rpath --set-rpath '$ORIGIN/../../php-firebird' pdo_fbird/modules/pdo_fbird.so

# Set RPATH on bundled libs ($ORIGIN so they find each other)
log "Setting RPATH on bundled libs..."
for lib in "${FB_ROOT}"/lib/*.so*; do
    if [ -f "$lib" ] && [ ! -L "$lib" ]; then
        patchelf --force-rpath --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
    fi
done

# Verify RPATH
log "RPATH verification:"
readelf -d modules/firebird.so | grep -E "RPATH|RUNPATH" || true

# -----------------------------------------------------------------------------
# Step 4: Prepare debian/ build directory
# -----------------------------------------------------------------------------

log_info "Preparing debian/ build directory..."

DEB_DIR="${REPO_ROOT}/debian"
# jane: remove stale debian/ dir (previous runs may have root-owned files)
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR"

# Copy packaging files from packaging/debian/ to debian/
cp -a packaging/debian/* "$DEB_DIR/"

# Get extension version
EXT_VERSION=$(cat VERSION.txt 2>/dev/null || echo "13.0.0-unknown")
log "Extension version: ${EXT_VERSION}"

# Update changelog with the correct version
cat > "${DEB_DIR}/changelog" << EOF
php-firebird (${EXT_VERSION}-1) ${DISTRO}; urgency=low

  * PHP ${PHP_VERSION} build for ${DISTRO} (${DEB_ARCH})
  * Bundled Firebird 5.0 client library (FB4+ features enabled)
  * RPATH \$ORIGIN/../../php-firebird for bundled lib resolution

 -- satware AG <info@satware.com>  $(date -R)
EOF

# Update control file: only keep the target PHP version package
# (dpkg-buildpackage builds all packages in control, but we only want one)
# jane: We keep all 4 packages in control for the APT repo, but the build
#       only produces one. The build.sh overrides the rules to target one PHP version.
export PHP_VERSION
export PHP_VER_SHORT
export FB_ROOT
export EXT_DIR
export DEB_ARCH

# Generate per-PHP-version maintainer scripts from templates.
# Debian Policy: postinst/prerm/postrm must be named debian/php<ver>-<ext>.<script>
# to be picked up by dpkg-buildpackage for the correct binary package.
# The templates use @PHP_VERSION@ placeholder (e.g. "8.4") which is substituted here.
log_info "Generating maintainer scripts for php${PHP_VERSION}-firebird..."
PKG_NAME="php${PHP_VERSION}-firebird"
for script in postinst prerm postrm; do
    TEMPLATE="${DEB_DIR}/php-firebird.${script}.in"
    OUTPUT="${DEB_DIR}/${PKG_NAME}.${script}"
    if [ -f "$TEMPLATE" ]; then
        sed "s/@PHP_VERSION@/${PHP_VERSION}/g" "$TEMPLATE" > "$OUTPUT"
        chmod 755 "$OUTPUT"
        log "  Generated ${OUTPUT}"
    else
        log_error "  Missing template: ${TEMPLATE}"
        exit 1
    fi
done

# Issue #501: declare the actual target architecture. A static Architecture
# list (amd64 arm64) makes debhelper silently skip the build on other arches
# ("No packages to build. Possible architecture mismatch: armhf, want: amd64 arm64").
sed -i "s/^Architecture: .*/Architecture: ${DEB_ARCH}/" "${DEB_DIR}/control"
log "  Architecture set to ${DEB_ARCH}"

# Make rules executable
chmod +x "${DEB_DIR}/rules"

# -----------------------------------------------------------------------------
# Step 5: Run dpkg-buildpackage
# -----------------------------------------------------------------------------

log_info "Running dpkg-buildpackage..."

# Check if dpkg-buildpackage is available
if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
    log_error "dpkg-buildpackage not found. Install with: apt-get install dpkg-dev debhelper"
    exit 1
fi

# Build the package
# -us -uc: skip GPG signing
# -b: binary-only build
dpkg-buildpackage -us -uc -b -d 2>&1 || {
    log_error "dpkg-buildpackage failed"
    # Show the last few lines of build log for debugging
    log_error "Build log tail:"
    dpkg-buildpackage -us -uc -b -d 2>&1 | tail -30
    exit 1
}

# -----------------------------------------------------------------------------
# Step 6: Find and report the .deb file
# -----------------------------------------------------------------------------

# dpkg-buildpackage puts the .deb in the parent directory of REPO_ROOT.
# jane: -maxdepth 1 is critical - without it, find searches the ENTIRE
# filesystem (including /src/dist/ which has stale .deb files from previous
# test runs). head -n 1 would pick the stale 13.0.0-1 file instead of the
# freshly built 13.0.1-rc.1-1 file. ! -name "*dbgsym*" excludes debug symbol
# packages that dpkg-buildpackage also produces.
DEB_FILE=$(find "${REPO_ROOT}/.." -maxdepth 1 -name "php${PHP_VERSION}-firebird_*.deb" -type f ! -name "*dbgsym*" | head -n 1)

if [ -z "$DEB_FILE" ]; then
    # Fallback: search in current directory (same constraints)
    DEB_FILE=$(find . -maxdepth 1 -name "php${PHP_VERSION}-firebird_*.deb" -type f ! -name "*dbgsym*" | head -n 1)
fi

if [ -z "$DEB_FILE" ]; then
    log_error "No .deb file found after build"
    log_error "Searching for any .deb files..."
    find "${REPO_ROOT}/.." -maxdepth 1 -name "*.deb" -type f ! -name "*dbgsym*" 2>/dev/null | head -10
    exit 1
fi

# Move to output directory
mkdir -p "$OUTPUT_DIR"
cp "$DEB_FILE" "$OUTPUT_DIR/"

# Generate checksum
sha256sum "$OUTPUT_DIR/$(basename "$DEB_FILE")" > "$OUTPUT_DIR/$(basename "$DEB_FILE").sha256"

# Clean up debian/ directory (don't leave it in the source tree)
rm -rf "$DEB_DIR"

# -----------------------------------------------------------------------------
# Report
# -----------------------------------------------------------------------------

cat << EOF

Build successful!

  PHP version:   ${PHP_VERSION}
  Distro:        ${DISTRO}
  Architecture:  ${DEB_ARCH}
  Extension ver: ${EXT_VERSION}
  Output:        ${OUTPUT_DIR}/$(basename "$DEB_FILE")
  Checksum:      ${OUTPUT_DIR}/$(basename "$DEB_FILE").sha256

Verify with:
  dpkg-deb -I ${OUTPUT_DIR}/$(basename "$DEB_FILE")   # Show package info
  dpkg-deb -c ${OUTPUT_DIR}/$(basename "$DEB_FILE")   # List contents
  lintian ${OUTPUT_DIR}/$(basename "$DEB_FILE")       # Check for issues

Install with:
  sudo dpkg -i ${OUTPUT_DIR}/$(basename "$DEB_FILE")
  php${PHP_VERSION} -m | grep firebird

EOF

#!/usr/bin/env bash
# =============================================================================
# packaging/rpm/build.sh
# =============================================================================
#
# Builds one .rpm package for php-firebird inside a Docker container.
#
# Usage:
#   packaging/rpm/build.sh --php-version 8.4 --distro el8 --arch x86_64
#
# This script is designed to run INSIDE a Docker container (e.g.,
# quay.io/pypa/manylinux_2_28_x86_64) that has:
#   - PHP dev headers (php-devel or built from source)
#   - Build tools (autoconf, gcc, g++, make, patchelf, rpm-build)
#
# The script:
#   1. Fetches the FB5 client via fetch-client.sh
#   2. Runs phpize + configure + make to build firebird.so + pdo_fbird.so
#   3. Sets RPATH on the extension .so files
#   4. Creates a source tarball and runs rpmbuild -bb
#   5. Outputs the path to the .rpm file
#
# =============================================================================

set -euo pipefail

# -----------------------------------------------------------------------------
# Defaults
# -----------------------------------------------------------------------------

PHP_VERSION="8.4"
DISTRO="el8"
ARCH="x86_64"
FB_ROOT="${FB_ROOT:-/opt/firebird}"
OUTPUT_DIR="${OUTPUT_DIR:-dist/rpm}"
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
    x86_64|amd64) ARCH="x86_64"; RPM_ARCH="x86_64" ;;
    aarch64|arm64) ARCH="aarch64"; RPM_ARCH="aarch64" ;;
    armv7l|armhf) ARCH="armv7l"; RPM_ARCH="armv7hl" ;;
    *) echo "ERROR: Unsupported arch: $ARCH" >&2; exit 1 ;;
esac

# Normalize distro for RPM %dist tag
case "$DISTRO" in
    el8|el9|fc41) : ;;
    *) echo "ERROR: Unsupported distro: $DISTRO (use el8, el9, or fc41)" >&2; exit 1 ;;
esac

log_info "Building php-firebird .rpm for PHP ${PHP_VERSION} on ${DISTRO} (${RPM_ARCH})"

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
# jane: same pattern as packaging/debian/build.sh - keeps source tree read-only.
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
#       Copy libtommath.so.1 from the system so it gets bundled in the .rpm.
#       libfbclient depends on libtommath at runtime.
#       Use cp -L (dereference symlinks) to copy the real file.
for libpath in /usr/lib64/libtommath.so* /usr/lib/libtommath.so*; do
    if ls $libpath >/dev/null 2>&1; then
        cp -L $libpath "${FB_ROOT}/lib/" 2>/dev/null || true
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

# jane: config.m4 uses $PHP_LIBDIR. manylinux uses lib64, FB client uses lib.
#       Create lib64 symlink so configure finds the libs.
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

# jane: The build verification proves the source compiles. The spec %build
#       section will rebuild from clean source and set RPATH in %install.
#       No RPATH/patchelf work needed here - the tarball ships clean source.

# -----------------------------------------------------------------------------
# Step 3: Create source tarball and rpmbuild tree
# -----------------------------------------------------------------------------

log_info "Preparing rpmbuild tree..."

EXT_VERSION=$(cat VERSION.txt 2>/dev/null || echo "13.0.0-unknown")
log "Extension version: ${EXT_VERSION}"

RPM_TOPDIR="${REPO_ROOT}/_rpmbuild"
rm -rf "$RPM_TOPDIR"
mkdir -p "$RPM_TOPDIR"/{SPECS,SOURCES,BUILD,RPMS,SRPMS}

# Copy spec file
cp packaging/rpm/php-firebird.spec "$RPM_TOPDIR/SPECS/"

# Create source tarball (GitHub archive format: php-firebird-VERSION/)
# jane: GitHub strips the leading 'v' from tag names in archive directories,
#       so the tarball dir must be php-firebird-13.0.3, not php-firebird-v13.0.3.
TARBALL_DIR="php-firebird-${EXT_VERSION}"
TARBALL="${RPM_TOPDIR}/SOURCES/${TARBALL_DIR}.tar.gz"

log "Creating source tarball: ${TARBALL}"
# Create tarball from repo root, excluding build artifacts and .git.
# jane: Exclude all build artifacts (modules/, configure, Makefile, etc.)
#       so the spec %build section rebuilds from clean source via phpize.
#       This is the standard RPM approach - tarball ships source, spec builds it.
tar czf "$TARBALL" \
    --transform "s,^,${TARBALL_DIR}/," \
    --exclude='.git' \
    --exclude='_rpmbuild' \
    --exclude='dist/' \
    --exclude='modules/' \
    --exclude='pdo_fbird/modules/' \
    --exclude='.libs' \
    --exclude='*.lo' \
    --exclude='*.la' \
    --exclude='*.o' \
    --exclude='*.dep' \
    --exclude='coverage/' \
    --exclude='vendor/' \
    --exclude='logs/' \
    --exclude='debian/' \
    --exclude='configure' \
    --exclude='config.h' \
    --exclude='config.h.in' \
    --exclude='config.log' \
    --exclude='config.status' \
    --exclude='config.nice' \
    --exclude='Makefile' \
    --exclude='Makefile.fragments' \
    --exclude='Makefile.global' \
    --exclude='Makefile.objects' \
    --exclude='pdo_fbird/Makefile' \
    --exclude='pdo_fbird/configure' \
    --exclude='pdo_fbird/config.h*' \
    . 2>/dev/null

# -----------------------------------------------------------------------------
# Step 4: Run rpmbuild
# -----------------------------------------------------------------------------

log_info "Running rpmbuild..."

if ! command -v rpmbuild >/dev/null 2>&1; then
    log_error "rpmbuild not found. Install with: yum install rpm-build"
    exit 1
fi

# jane: --nodeps skips dependency resolution. The FB5 client is fetched
#       by build.sh, not installed as an RPM package. Without --nodeps,
#       rpmbuild fails on the file-based BuildRequires for libfbclient.so.
rpmbuild -bb \
    --nodeps \
    --define "_topdir ${RPM_TOPDIR}" \
    --define "php_version ${PHP_VERSION}" \
    --define "fb_root ${FB_ROOT}" \
    --define "dist .${DISTRO}" \
    --target "${RPM_ARCH}" \
    "$RPM_TOPDIR/SPECS/php-firebird.spec" 2>&1 || {
    log_error "rpmbuild failed"
    exit 1
}

# -----------------------------------------------------------------------------
# Step 5: Find and report the .rpm file
# -----------------------------------------------------------------------------

RPM_FILE=$(find "${RPM_TOPDIR}/RPMS/${RPM_ARCH}" -name "php-firebird-*.rpm" -type f | head -n 1)

if [ -z "$RPM_FILE" ]; then
    log_error "No .rpm file found after build"
    find "${RPM_TOPDIR}/RPMS" -name "*.rpm" -type f 2>/dev/null | head -10
    exit 1
fi

# Move to output directory
mkdir -p "$OUTPUT_DIR"
cp "$RPM_FILE" "$OUTPUT_DIR/"

# Generate checksum
sha256sum "$OUTPUT_DIR/$(basename "$RPM_FILE")" > "$OUTPUT_DIR/$(basename "$RPM_FILE").sha256"

# Clean up rpmbuild tree
rm -rf "$RPM_TOPDIR"

# -----------------------------------------------------------------------------
# Report
# -----------------------------------------------------------------------------

cat << EOF

Build successful!

  PHP version:   ${PHP_VERSION}
  Distro:        ${DISTRO}
  Architecture:  ${RPM_ARCH}
  Extension ver: ${EXT_VERSION}
  Output:        ${OUTPUT_DIR}/$(basename "$RPM_FILE")
  Checksum:      ${OUTPUT_DIR}/$(basename "$RPM_FILE").sha256

Verify with:
  rpm -qip ${OUTPUT_DIR}/$(basename "$RPM_FILE")   # Show package info
  rpm -qlp ${OUTPUT_DIR}/$(basename "$RPM_FILE")   # List contents
  rpmlint ${OUTPUT_DIR}/$(basename "$RPM_FILE")    # Check for issues

Install with:
  sudo rpm -i ${OUTPUT_DIR}/$(basename "$RPM_FILE")
  php -m | grep firebird

EOF

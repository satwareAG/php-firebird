#!/bin/bash
# =============================================================================
# Extract Firebird SDK from macOS .pkg
# =============================================================================
#
# Downloads and extracts Firebird headers and libraries from the official
# macOS .pkg installer for use in building the PHP Firebird extension.
#
# The macOS .pkg installs Firebird as a framework at:
#   /Library/Frameworks/Firebird.framework/
#
# Key issue: The .pkg's Headers directory is flat - it does NOT include the
# nested impl/ subdirectory containing types_pub.h. This script reconstructs
# the full header tree required for compilation.
#
# Usage:
#   ./extract-firebird-macos.sh --arch arm64|x64 [options]
#
# Options:
#   --arch ARCH          Target architecture: arm64 or x64 (required)
#   --fb-version VER     Firebird version [default: 5.0.3]
#   --fb-build BUILD     Firebird build number [default: 1683]
#   --output DIR         Output directory [default: /opt/firebird]
#   --pkg-path PATH      Use local .pkg file instead of downloading
#   --help               Show this help message
#
# Output structure:
#   <output>/include/ibase.h
#   <output>/include/iberror.h
#   <output>/include/firebird/impl/types_pub.h  (critical - missing from .pkg)
#   <output>/lib/libfbclient.dylib
#
# =============================================================================

set -euo pipefail

# =============================================================================
# Configuration
# =============================================================================

ARCH=""
FB_VERSION="${FB_VERSION:-5.0.3}"
FB_BUILD="${FB_BUILD:-1683}"
OUTPUT_DIR="/opt/firebird"
PKG_PATH=""

# =============================================================================
# Helpers
# =============================================================================

usage() {
    sed -n '3,30p' "$0" | sed 's/^# //' | sed 's/^#//'
    exit 0
}

log_info() {
    echo ">>> $*"
}

log_error() {
    echo "ERROR: $*" >&2
}

die() {
    log_error "$@"
    exit 1
}

# =============================================================================
# Argument Parsing
# =============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --arch)       ARCH="$2"; shift 2 ;;
        --fb-version) FB_VERSION="$2"; shift 2 ;;
        --fb-build)   FB_BUILD="$2"; shift 2 ;;
        --output)     OUTPUT_DIR="$2"; shift 2 ;;
        --pkg-path)   PKG_PATH="$2"; shift 2 ;;
        --help|-h)    usage ;;
        *)            die "Unknown option: $1" ;;
    esac
done

if [ -z "$ARCH" ]; then
    die "Architecture required. Use --arch arm64 or --arch x64"
fi

# Map arch to Firebird .pkg suffix
case "$ARCH" in
    arm64)  FB_PKG_SUFFIX="macos-arm64" ;;
    x64)    FB_PKG_SUFFIX="macos-x64" ;;
    x86_64) FB_PKG_SUFFIX="macos-x64"; ARCH="x64" ;;
    *)      die "Unsupported architecture: $ARCH (use arm64 or x64)" ;;
esac

FB_PKG_NAME="Firebird-${FB_VERSION}.${FB_BUILD}-0-${FB_PKG_SUFFIX}.pkg"

# =============================================================================
# Download .pkg (if not provided)
# =============================================================================

if [ -n "$PKG_PATH" ]; then
    [ -f "$PKG_PATH" ] || die "Package not found: $PKG_PATH"
    log_info "Using local package: $PKG_PATH"
else
    PKG_PATH="/tmp/firebird-cache/${FB_PKG_NAME}"

    if [ -f "$PKG_PATH" ]; then
        log_info "Using cached package: $PKG_PATH"
    else
        log_info "Downloading Firebird ${FB_VERSION} for ${FB_PKG_SUFFIX}..."
        mkdir -p /tmp/firebird-cache

        FB_URL="https://github.com/FirebirdSQL/firebird/releases/download/v${FB_VERSION}/${FB_PKG_NAME}"
        DELAY=10

        for attempt in 1 2 3 4 5; do
            echo "Download attempt ${attempt}/5..."
            if curl -fsSL --retry 3 --retry-delay 5 --retry-connrefused \
                ${GITHUB_TOKEN:+-H "Authorization: token ${GITHUB_TOKEN}"} \
                "${FB_URL}" -o "$PKG_PATH"; then
                FSIZE=$(stat -f%z "$PKG_PATH" 2>/dev/null || stat -c%s "$PKG_PATH" 2>/dev/null || echo "0")
                if [ "${FSIZE}" -gt 1000000 ]; then
                    log_info "Download OK (${FSIZE} bytes)"
                    break
                fi
                rm -f "$PKG_PATH"
            fi
            [ "$attempt" -lt 5 ] && echo "Sleeping ${DELAY}s..." && sleep "$DELAY"
            DELAY=$((DELAY * 2))
        done

        [ -f "$PKG_PATH" ] || die "All download attempts failed for ${FB_URL}"
    fi
fi

# =============================================================================
# Extract .pkg
# =============================================================================

log_info "Extracting .pkg..."

PKG_EXPANDED="/tmp/firebird-expanded"
EXTRACT_DIR="/tmp/firebird-payload"
rm -rf "$PKG_EXPANDED" "$EXTRACT_DIR"
mkdir -p "$EXTRACT_DIR"

# pkgutil --expand-full extracts everything including payloads
# (available on macOS 10.6+, handles both flat and distribution packages)
if command -v pkgutil >/dev/null 2>&1; then
    # Try --expand-full first (extracts Payload automatically)
    if pkgutil --expand-full "$PKG_PATH" "$PKG_EXPANDED" 2>/dev/null; then
        log_info "Expanded with pkgutil --expand-full"
    else
        # Fallback to --expand + manual Payload extraction
        pkgutil --expand "$PKG_PATH" "$PKG_EXPANDED"
        log_info "Expanded with pkgutil --expand, extracting payloads manually..."

        find "$PKG_EXPANDED" -name "Payload" | while read -r payload; do
            echo "Extracting payload: ${payload}"
            cd "$EXTRACT_DIR"
            if file "$payload" | grep -q "gzip"; then
                gunzip -c "$payload" | cpio -idm 2>/dev/null || true
            elif file "$payload" | grep -q "cpio"; then
                cpio -idm < "$payload" 2>/dev/null || true
            else
                tar xf "$payload" 2>/dev/null || true
            fi
        done
    fi
else
    die "pkgutil not found - this script must run on macOS"
fi

# Merge expanded content into extract dir
# --expand-full puts payload contents directly in component dirs
if [ -d "$PKG_EXPANDED" ]; then
    # Find Payload directories from --expand-full
    find "$PKG_EXPANDED" -name "Payload" -type d | while read -r payload_dir; do
        log_info "Copying payload from: $payload_dir"
        cp -a "$payload_dir"/* "$EXTRACT_DIR/" 2>/dev/null || true
    done

    # Also check for directly extracted framework (--expand-full puts files here)
    find "$PKG_EXPANDED" -path "*/Firebird.framework" -type d | head -1 | while read -r fw; do
        log_info "Found framework in expanded dir: $fw"
        mkdir -p "$EXTRACT_DIR/Library/Frameworks"
        cp -a "$fw" "$EXTRACT_DIR/Library/Frameworks/" 2>/dev/null || true
    done
fi

echo "=== Extracted payload structure ==="
find "$EXTRACT_DIR" -maxdepth 5 -type f | head -40

# =============================================================================
# Locate Firebird Framework
# =============================================================================

log_info "Locating Firebird framework..."

FB_FRAMEWORK=$(find "$EXTRACT_DIR" -path "*/Firebird.framework" -type d 2>/dev/null | head -1)

if [ -z "$FB_FRAMEWORK" ]; then
    # Also search in the expanded pkg directory itself
    FB_FRAMEWORK=$(find "$PKG_EXPANDED" -path "*/Firebird.framework" -type d 2>/dev/null | head -1)
fi

if [ -z "$FB_FRAMEWORK" ]; then
    log_error "Firebird.framework not found in extracted payload"
    echo "Full payload tree:"
    find "$EXTRACT_DIR" -type f | head -60
    find "$PKG_EXPANDED" -type f | head -60
    die "Cannot proceed without Firebird.framework"
fi

log_info "Found Firebird framework: $FB_FRAMEWORK"

# =============================================================================
# Copy Headers and Libraries to Output
# =============================================================================

log_info "Installing to $OUTPUT_DIR..."

mkdir -p "$OUTPUT_DIR/include/firebird/impl" "$OUTPUT_DIR/lib"

# --- Headers ---
# The framework Headers/ contains ibase.h, iberror.h, etc. but NOT the
# nested firebird/impl/ subdirectory with types_pub.h
HEADERS_DIR=""
if [ -d "$FB_FRAMEWORK/Headers" ]; then
    HEADERS_DIR="$FB_FRAMEWORK/Headers"
elif [ -d "$FB_FRAMEWORK/Versions/A/Headers" ]; then
    HEADERS_DIR="$FB_FRAMEWORK/Versions/A/Headers"
fi

if [ -n "$HEADERS_DIR" ] && [ -d "$HEADERS_DIR" ]; then
    log_info "Copying headers from: $HEADERS_DIR"
    cp -a "$HEADERS_DIR"/* "$OUTPUT_DIR/include/"

    # CRITICAL FIX: Reconstruct the firebird/impl/ header tree
    # ibase.h includes <firebird/impl/types_pub.h> but the .pkg doesn't
    # ship the nested directory structure. We need to find types_pub.h
    # and place it in the correct location.
    if [ ! -f "$OUTPUT_DIR/include/firebird/impl/types_pub.h" ]; then
        log_info "Reconstructing firebird/impl/ header tree..."

        # Search for types_pub.h in the entire extracted tree
        TYPES_PUB=$(find "$EXTRACT_DIR" "$PKG_EXPANDED" -name "types_pub.h" -type f 2>/dev/null | head -1)

        if [ -n "$TYPES_PUB" ]; then
            cp "$TYPES_PUB" "$OUTPUT_DIR/include/firebird/impl/types_pub.h"
            log_info "Found types_pub.h at: $TYPES_PUB"
        else
            # types_pub.h may be embedded in the flat Headers/ dir
            # or we need to extract it from the ibase.h include path
            # As a last resort, check if it's in the Headers dir under a different path
            TYPES_PUB=$(find "$FB_FRAMEWORK" -name "types_pub.h" -type f 2>/dev/null | head -1)
            if [ -n "$TYPES_PUB" ]; then
                cp "$TYPES_PUB" "$OUTPUT_DIR/include/firebird/impl/types_pub.h"
                log_info "Found types_pub.h in framework: $TYPES_PUB"
            fi
        fi

        # If types_pub.h is in the flat include dir, move it to the nested path
        if [ -f "$OUTPUT_DIR/include/types_pub.h" ] && \
           [ ! -f "$OUTPUT_DIR/include/firebird/impl/types_pub.h" ]; then
            cp "$OUTPUT_DIR/include/types_pub.h" "$OUTPUT_DIR/include/firebird/impl/types_pub.h"
            log_info "Copied flat types_pub.h to firebird/impl/"
        fi

        # Copy any other impl/ headers found
        find "$EXTRACT_DIR" "$PKG_EXPANDED" -path "*/impl/*.h" -type f 2>/dev/null | while read -r hdr; do
            BASENAME=$(basename "$hdr")
            if [ ! -f "$OUTPUT_DIR/include/firebird/impl/$BASENAME" ]; then
                cp "$hdr" "$OUTPUT_DIR/include/firebird/impl/$BASENAME"
                log_info "Copied impl header: $BASENAME"
            fi
        done
    fi

    # Also ensure firebird/ directory symlink for includes that use <firebird/...>
    # Some headers include via firebird/Interface.h or similar paths
    find "$EXTRACT_DIR" "$PKG_EXPANDED" -name "Interface.h" -path "*/firebird/*" -type f 2>/dev/null | head -1 | while read -r iface; do
        IFACE_DIR=$(dirname "$iface")
        log_info "Found firebird interface headers at: $IFACE_DIR"
        cp -a "$IFACE_DIR"/* "$OUTPUT_DIR/include/firebird/" 2>/dev/null || true
    done
else
    log_info "No standard Headers/ dir found, searching for loose headers..."
    find "$EXTRACT_DIR" "$PKG_EXPANDED" -name "ibase.h" -type f -exec cp {} "$OUTPUT_DIR/include/" \;
    find "$EXTRACT_DIR" "$PKG_EXPANDED" -name "iberror.h" -type f -exec cp {} "$OUTPUT_DIR/include/" \;
    find "$EXTRACT_DIR" "$PKG_EXPANDED" -name "types_pub.h" -type f | head -1 | while read -r f; do
        cp "$f" "$OUTPUT_DIR/include/firebird/impl/"
    done
fi

# --- Libraries ---
log_info "Copying libraries..."

# Find dylibs in the framework
find "$FB_FRAMEWORK" -name "*.dylib" -type f -exec cp {} "$OUTPUT_DIR/lib/" \;

# The framework binary itself is libfbclient
FB_BINARY=""
if [ -f "$FB_FRAMEWORK/Versions/A/Firebird" ]; then
    FB_BINARY="$FB_FRAMEWORK/Versions/A/Firebird"
elif [ -f "$FB_FRAMEWORK/Firebird" ]; then
    FB_BINARY="$FB_FRAMEWORK/Firebird"
fi

if [ -n "$FB_BINARY" ] && [ -f "$FB_BINARY" ]; then
    # Check if this is actually a Mach-O binary (not a symlink to directory)
    if file "$FB_BINARY" | grep -q "Mach-O"; then
        if [ ! -f "$OUTPUT_DIR/lib/libfbclient.dylib" ]; then
            cp "$FB_BINARY" "$OUTPUT_DIR/lib/libfbclient.dylib"
            log_info "Copied framework binary as libfbclient.dylib"
        fi
    fi
fi

# Also search for libfbclient directly in extracted payload
find "$EXTRACT_DIR" -name "libfbclient*" -type f 2>/dev/null | while read -r lib; do
    LIBNAME=$(basename "$lib")
    if [ ! -f "$OUTPUT_DIR/lib/$LIBNAME" ]; then
        cp "$lib" "$OUTPUT_DIR/lib/$LIBNAME"
        log_info "Copied: $LIBNAME"
    fi
done

# Create version symlinks
cd "$OUTPUT_DIR/lib"
for lib in libfbclient.dylib.*; do
    if [ -f "$lib" ] && [ ! -L "$lib" ]; then
        base="${lib%%.*}"
        [ ! -e "${base}.dylib" ] && ln -sf "$lib" "${base}.dylib" 2>/dev/null || true
    fi
done 2>/dev/null || true

# =============================================================================
# Verification
# =============================================================================

log_info "Verifying installation..."

echo "=== Headers ==="
ls -la "$OUTPUT_DIR/include/" || true
echo ""

if [ -d "$OUTPUT_DIR/include/firebird" ]; then
    echo "=== Nested headers (firebird/) ==="
    find "$OUTPUT_DIR/include/firebird" -type f | head -10
    echo ""
fi

echo "=== Libraries ==="
ls -la "$OUTPUT_DIR/lib/" || true
echo ""

# Critical checks
ERRORS=0

if [ ! -f "$OUTPUT_DIR/include/ibase.h" ]; then
    log_error "CRITICAL: ibase.h not found"
    ERRORS=$((ERRORS + 1))
fi

if [ ! -f "$OUTPUT_DIR/include/firebird/impl/types_pub.h" ]; then
    log_error "CRITICAL: firebird/impl/types_pub.h not found"
    log_error "This will cause 'impl/types_pub.h file not found' build errors"
    ERRORS=$((ERRORS + 1))
fi

if ! ls "$OUTPUT_DIR/lib/libfbclient"* >/dev/null 2>&1; then
    log_error "CRITICAL: libfbclient not found"
    ERRORS=$((ERRORS + 1))
fi

if [ "$ERRORS" -gt 0 ]; then
    die "$ERRORS critical check(s) failed"
fi

# Show library architecture
for dylib in "$OUTPUT_DIR"/lib/*.dylib; do
    if [ -f "$dylib" ] && [ ! -L "$dylib" ]; then
        echo "Architecture of $(basename "$dylib"):"
        file "$dylib"
        lipo -info "$dylib" 2>/dev/null || true
    fi
done

# =============================================================================
# Cleanup
# =============================================================================

rm -rf "$PKG_EXPANDED" "$EXTRACT_DIR"

log_info "Firebird SDK extracted successfully to $OUTPUT_DIR"
log_info "  Headers: $OUTPUT_DIR/include/"
log_info "  Libraries: $OUTPUT_DIR/lib/"

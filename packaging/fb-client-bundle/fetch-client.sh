#!/usr/bin/env bash
# =============================================================================
# packaging/fb-client-bundle/fetch-client.sh
# =============================================================================
#
# Downloads and extracts the official Firebird client SDK (libfbclient + headers)
# for use by native packaging build scripts (.deb, .rpm, .apk, PIE).
#
# Bundling the FB5+ client (instead of depending on system `firebird-dev`) is
# REQUIRED so that `#if FB_API_VER >= 40` code paths (DECFLOAT, INT128, time
# zones, batch DML, pdo_fbird) are compiled in. Debian 12/Ubuntu 22.04 ship
# Firebird 3.0 client in apt, which compiles out all FB4+ features.
#
# Usage:
#   packaging/fb-client-bundle/fetch-client.sh --arch <x86_64|aarch64|armv7l> \
#       --output-dir <path> [--fb-major <3|4|5>] [--cache-dir <path>] [--verbose]
#
# Examples:
#   packaging/fb-client-bundle/fetch-client.sh --arch x86_64 --output-dir /opt/firebird
#   packaging/fb-client-bundle/fetch-client.sh --arch aarch64 --output-dir /tmp/fb --verbose
#
# Output directory layout:
#   <output-dir>/
#     lib/
#       libfbclient.so.5.0.3.1683    # Real file
#       libfbclient.so.5              # Symlink -> libfbclient.so.5.0.3.1683
#       libfbclient.so                # Symlink -> libfbclient.so.5
#       libicu*.so.*                  # ICU libs (libfbclient dep)
#       libtomcrypt.so.*              # TomCrypt (libfbclient dep)
#       libtommath.so.*               # TomMath (libfbclient dep)
#       libre2.so.*                   # RE2 (libfbclient dep)
#     include/
#       ibase.h                       # Public API header
#       firebird/                     # Nested headers (impl/, MessageInterface.h, etc.)
#
# Caching:
#   Downloads are cached in --cache-dir (default: /tmp/fb-client-cache/).
#   Re-runs with the same arch + version skip the download.
#
# =============================================================================

set -euo pipefail

# -----------------------------------------------------------------------------
# Defaults
# -----------------------------------------------------------------------------

ARCH=""
OUTPUT_DIR=""
FB_MAJOR="5"
CACHE_DIR="${FB_CLIENT_CACHE_DIR:-/tmp/fb-client-cache}"
VERBOSE=0

# -----------------------------------------------------------------------------
# Argument parsing
# -----------------------------------------------------------------------------

usage() {
    sed -n '3,40p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
}

while [ $# -gt 0 ]; do
    case "$1" in
        --arch)
            ARCH="$2"; shift 2 ;;
        --arch=*)
            ARCH="${1#--arch=}"; shift ;;
        --output-dir)
            OUTPUT_DIR="$2"; shift 2 ;;
        --output-dir=*)
            OUTPUT_DIR="${1#--output-dir=}"; shift ;;
        --fb-major)
            FB_MAJOR="$2"; shift 2 ;;
        --fb-major=*)
            FB_MAJOR="${1#--fb-major=}"; shift ;;
        --cache-dir)
            CACHE_DIR="$2"; shift 2 ;;
        --cache-dir=*)
            CACHE_DIR="${1#--cache-dir=}"; shift ;;
        --verbose|-v)
            VERBOSE=1; shift ;;
        --help|-h)
            usage ;;
        *)
            echo "ERROR: Unknown argument: $1" >&2
            exit 1 ;;
    esac
done

# -----------------------------------------------------------------------------
# Validation
# -----------------------------------------------------------------------------

if [ -z "$ARCH" ]; then
    echo "ERROR: --arch is required (one of: x86_64, aarch64, armv7l)" >&2
    exit 1
fi

if [ -z "$OUTPUT_DIR" ]; then
    echo "ERROR: --output-dir is required" >&2
    exit 1
fi

# Map architecture to Firebird asset keyword (validated and resolved inline below)
# FB5 Linux tarballs: linux-x64.tar.gz (x86_64), linux-arm64.tar.gz (aarch64)
# FB3/FB4 Linux tarballs: amd64.tar.gz (x86_64 only - no ARM builds for FB3/FB4)
case "$ARCH" in
    x86_64|amd64) ;;
    aarch64|arm64) ;;
    armv7l|armhf)
        # FB >= 5.0.3 ships official linux-arm32 tarballs (#502) - the old
        # "must build from source" note is outdated. Validated for FB5 only.
        ;;
    *)
        echo "ERROR: Unsupported arch: $ARCH (expected: x86_64, aarch64, armv7l)" >&2
        exit 1 ;;
esac

# aarch64 only supported on FB5+
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    if [ "$FB_MAJOR" != "5" ]; then
        echo "ERROR: aarch64 requires --fb-major 5 (FB3/FB4 have no ARM tarballs)" >&2
        exit 1
    fi
fi

# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

log() {
    if [ "$VERBOSE" -eq 1 ]; then
        echo ">>> $*" >&2
    fi
}

# -----------------------------------------------------------------------------
# Resolve Firebird release (arch-aware, inline)
# -----------------------------------------------------------------------------
# jane: does not call scripts/get-latest-firebird.sh because that script
# hardcodes linux-x64 for FB5 and has no --arch parameter. Inline resolution
# here is arch-aware.

log "Resolving latest Firebird ${FB_MAJOR}.x release for arch ${ARCH}..."

GITHUB_API="https://api.github.com/repos/FirebirdSQL/firebird"
CURL_OPTS=(-s -f --retry 3 --retry-delay 5)
if [ -n "${GITHUB_TOKEN:-}" ]; then
    CURL_OPTS+=(-H "Authorization: token ${GITHUB_TOKEN}")
fi

# Determine asset keyword based on FB major version and arch
# FB5: linux-x64.tar.gz (x86_64), linux-arm64.tar.gz (aarch64)
# FB3/FB4: amd64.tar.gz (x86_64 only; no official ARM builds)
case "$FB_MAJOR" in
    3|4)
        if [ "$ARCH" != "x86_64" ] && [ "$ARCH" != "amd64" ]; then
            echo "ERROR: FB${FB_MAJOR} has no official ARM tarballs (only x86_64). Use FB5 for ARM." >&2
            exit 1
        fi
        ASSET_KEYWORD="amd64"
        ;;
    5)
        case "$ARCH" in
            x86_64|amd64) ASSET_KEYWORD="linux-x64" ;;
            aarch64|arm64) ASSET_KEYWORD="linux-arm64" ;;
            armv7l|armhf) ASSET_KEYWORD="linux-arm32" ;;
            *)
                echo "ERROR: Unsupported arch for FB5: $ARCH" >&2
                exit 1 ;;
        esac
        ;;
    *)
        echo "ERROR: Unsupported FB major version: $FB_MAJOR (expected 3, 4, or 5)" >&2
        exit 1
        ;;
esac

# Resolve latest tag for the major version (exclude Beta/RC)
TAG=$(curl "${CURL_OPTS[@]}" "${GITHUB_API}/releases" | \
    jq -r '.[].tag_name' | \
    grep -E "^v${FB_MAJOR}\." | \
    grep -vE "(Beta|RC|Release)" | \
    sort -V | tail -n 1)

if [ -z "$TAG" ]; then
    echo "ERROR: Could not resolve latest tag for Firebird ${FB_MAJOR}" >&2
    exit 1
fi

RELEASE_JSON=$(curl "${CURL_OPTS[@]}" "${GITHUB_API}/releases/tags/${TAG}")

# Match asset by arch keyword, exclude debugSymbols/debuginfo
# jane: jq test() takes a JSON string; backslash must be double-escaped (\\.)
ASSET_MATCH="${ASSET_KEYWORD}\\\\.tar\\\\.gz$"
ASSET_NAME=$(echo "$RELEASE_JSON" | jq -r ".assets[].name | select(test(\"${ASSET_MATCH}\") and (test(\"(debugSymbols|debuginfo)\") | not))" | head -n 1)
ASSET_URL=$(echo "$RELEASE_JSON" | jq -r ".assets[] | select(.name == \"${ASSET_NAME}\") | .browser_download_url")

if [ -z "$ASSET_URL" ]; then
    echo "ERROR: Could not find asset matching '${ASSET_KEYWORD}' for ${TAG}" >&2
    echo "Available assets:" >&2
    echo "$RELEASE_JSON" | jq -r '.assets[].name' >&2
    exit 1
fi

FB_VERSION="${TAG#v}"
FB_URL="$ASSET_URL"
FB_TARBALL="$ASSET_NAME"
FB_EXTRACT_DIR="${ASSET_NAME%.tar.gz}"

log "Resolved: Firebird ${FB_VERSION} (${FB_TARBALL})"
log "URL: ${FB_URL}"
log "Extract dir: ${FB_EXTRACT_DIR}"

# -----------------------------------------------------------------------------
# Download (with cache)
# -----------------------------------------------------------------------------

mkdir -p "$CACHE_DIR"
TARBALL_PATH="${CACHE_DIR}/${FB_TARBALL}"

if [ -f "$TARBALL_PATH" ]; then
    FSIZE=$(stat -c%s "$TARBALL_PATH" 2>/dev/null || stat -f%z "$TARBALL_PATH" 2>/dev/null || echo "0")
    if [ "$FSIZE" -gt 8000000 ]; then
        log "Cache hit: $TARBALL_PATH (${FSIZE} bytes)"
    else
        log "Cache file too small (${FSIZE} bytes), re-downloading"
        rm -f "$TARBALL_PATH"
    fi
fi

if [ ! -f "$TARBALL_PATH" ]; then
    log "Downloading to $TARBALL_PATH ..."
    DELAY=10
    for attempt in 1 2 3 4 5; do
        echo "Downloading Firebird client (attempt ${attempt}/5)..." >&2
        CURL_ARGS=(-fsSL --retry 3 --retry-delay 5 --retry-connrefused)
        if [ -n "${GITHUB_TOKEN:-}" ]; then
            CURL_ARGS+=(-H "Authorization: token ${GITHUB_TOKEN}")
        fi
        if curl "${CURL_ARGS[@]}" "$FB_URL" -o "$TARBALL_PATH"; then
            FSIZE=$(stat -c%s "$TARBALL_PATH" 2>/dev/null || echo "0")
            if [ "$FSIZE" -gt 8000000 ]; then
                echo "Download OK (${FSIZE} bytes)" >&2
                break
            fi
            echo "File too small (${FSIZE} bytes), discarding" >&2
            rm -f "$TARBALL_PATH"
        fi
        [ "$attempt" -lt 5 ] && { echo "Sleeping ${DELAY}s..." >&2; sleep "$DELAY"; }
        DELAY=$((DELAY * 2))
    done

    [ -f "$TARBALL_PATH" ] || { echo "ERROR: All download attempts failed" >&2; exit 1; }
fi

# -----------------------------------------------------------------------------
# Extract
# -----------------------------------------------------------------------------

TMP_EXTRACT=$(mktemp -d)
trap 'rm -rf "$TMP_EXTRACT"' EXIT

log "Extracting outer tarball to $TMP_EXTRACT ..."
tar -xzf "$TARBALL_PATH" -C "$TMP_EXTRACT"

# Linux FB5 tarballs have nested structure: <dir>/buildroot.tar.gz
# buildroot.tar.gz contains opt/firebird/{lib,include}
INNER_DIR="${TMP_EXTRACT}/${FB_EXTRACT_DIR:-${EXTRACT_DIR}}"
if [ ! -d "$INNER_DIR" ]; then
    # Fallback: find the first directory
    INNER_DIR=$(find "$TMP_EXTRACT" -maxdepth 1 -type d | tail -n 1)
fi

BUILDROOT_TGZ="${INNER_DIR}/buildroot.tar.gz"
if [ -f "$BUILDROOT_TGZ" ]; then
    log "Extracting nested buildroot.tar.gz ..."
    tar -xzf "$BUILDROOT_TGZ" -C "$INNER_DIR"
else
    echo "WARNING: No nested buildroot.tar.gz found (FB3 layout?)" >&2
fi

# Locate the lib/ and include/ directories
FB_LIB_DIR=""
FB_INCLUDE_DIR=""

# Try buildroot structure first: opt/firebird/lib
for candidate in \
    "${INNER_DIR}/opt/firebird/lib" \
    "${INNER_DIR}/opt/firebird/lib64" \
    "$(find "$INNER_DIR" -type d -name lib -path '*firebird*' 2>/dev/null | head -n 1)" \
    "$(find "$INNER_DIR" -name 'libfbclient.so*' -printf '%h\n' 2>/dev/null | head -n 1)"; do
    if [ -n "$candidate" ] && [ -d "$candidate" ]; then
        if ls "$candidate"/libfbclient.so* >/dev/null 2>&1; then
            FB_LIB_DIR="$candidate"
            break
        fi
    fi
done

for candidate in \
    "${INNER_DIR}/opt/firebird/include" \
    "$(find "$INNER_DIR" -type d -name include -path '*firebird*' 2>/dev/null | head -n 1)" \
    "$(find "$INNER_DIR" -name 'ibase.h' -printf '%h\n' 2>/dev/null | head -n 1)"; do
    if [ -n "$candidate" ] && [ -d "$candidate" ]; then
        if [ -f "$candidate/ibase.h" ]; then
            FB_INCLUDE_DIR="$candidate"
            break
        fi
    fi
done

if [ -z "$FB_LIB_DIR" ]; then
    echo "ERROR: Could not find libfbclient.so* in extracted tarball" >&2
    echo "Searched in: $INNER_DIR" >&2
    find "$INNER_DIR" -name 'libfbclient*' 2>&1 | head -10 >&2
    exit 1
fi

if [ -z "$FB_INCLUDE_DIR" ]; then
    echo "ERROR: Could not find ibase.h in extracted tarball" >&2
    exit 1
fi

log "Found libs in: $FB_LIB_DIR"
log "Found headers in: $FB_INCLUDE_DIR"

# -----------------------------------------------------------------------------
# Install to output dir
# -----------------------------------------------------------------------------

mkdir -p "$OUTPUT_DIR/lib" "$OUTPUT_DIR/include"

log "Copying libs to ${OUTPUT_DIR}/lib/ ..."
cp -a "$FB_LIB_DIR"/libfbclient.so* "$OUTPUT_DIR/lib/" 2>/dev/null || true
cp -a "$FB_LIB_DIR"/libicu*.so* "$OUTPUT_DIR/lib/" 2>/dev/null || true
cp -a "$FB_LIB_DIR"/libtomcrypt.so* "$OUTPUT_DIR/lib/" 2>/dev/null || true
cp -a "$FB_LIB_DIR"/libtommath.so* "$OUTPUT_DIR/lib/" 2>/dev/null || true
cp -a "$FB_LIB_DIR"/libre2.so* "$OUTPUT_DIR/lib/" 2>/dev/null || true

log "Copying headers to ${OUTPUT_DIR}/include/ ..."
cp -r "$FB_INCLUDE_DIR"/* "$OUTPUT_DIR/include/"

# -----------------------------------------------------------------------------
# Ensure symlink chain: libfbclient.so -> libfbclient.so.5 -> libfbclient.so.5.0.x
# -----------------------------------------------------------------------------

cd "$OUTPUT_DIR/lib"

# Find the real file (highest-versioned .so without symlink target)
REAL_FILE=$(find . -maxdepth 1 -name 'libfbclient.so.*' -type f ! -type l 2>/dev/null | sort -V | tail -n 1)
if [ -z "$REAL_FILE" ]; then
    # Some tarballs ship the real file as libfbclient.so.2.* (soname)
    REAL_FILE=$(find . -maxdepth 1 -name 'libfbclient.so.*' -type f 2>/dev/null | sort -V | tail -n 1)
fi

if [ -n "$REAL_FILE" ]; then
    REAL_BASE=$(basename "$REAL_FILE")

    # Determine the actual SONAME from the binary itself.
    # Firebird's libfbclient uses soname "libfbclient.so.2" for ABI compat
    # across all FB 2.x-5.x releases. Guessing from the version string
    # (e.g. libfbclient.so.5 for FB5) would be WRONG.
    ACTUAL_SONAME=""
    if command -v readelf >/dev/null 2>&1; then
        ACTUAL_SONAME=$(readelf -d "$REAL_FILE" 2>/dev/null | \
            grep -E 'Library soname:' | \
            sed -E 's/.*Library soname: \[([^]]+)\].*/\1/')
    fi

    # Fallback: if the tarball already has a soname symlink, use it
    if [ -z "$ACTUAL_SONAME" ]; then
        ACTUAL_SONAME=$(find . -maxdepth 1 -name 'libfbclient.so.*' -type l 2>/dev/null | \
            sort -V | tail -n 1 | xargs -I{} basename {} 2>/dev/null || true)
    fi

    # Final fallback: guess from filename (last resort, may be wrong)
    if [ -z "$ACTUAL_SONAME" ]; then
        ACTUAL_SONAME=$(echo "$REAL_BASE" | sed -E 's/^(libfbclient\.so\.[0-9]+).*/\1/')
        echo "WARNING: Could not determine SONAME via readelf; guessing: $ACTUAL_SONAME" >&2
    fi

    log "Detected SONAME: $ACTUAL_SONAME (from $REAL_BASE)"

    # Ensure SONAME symlink exists
    if [ "$REAL_BASE" != "$ACTUAL_SONAME" ] && [ ! -e "$ACTUAL_SONAME" ]; then
        ln -sf "$REAL_BASE" "$ACTUAL_SONAME"
    fi

    # Ensure libfbclient.so dev symlink exists -> SONAME
    if [ ! -e "libfbclient.so" ]; then
        ln -sf "$ACTUAL_SONAME" "libfbclient.so"
    fi

    log "Symlink chain: libfbclient.so -> $ACTUAL_SONAME -> $REAL_BASE"
fi

# -----------------------------------------------------------------------------
# Verify
# -----------------------------------------------------------------------------

if [ ! -f "$OUTPUT_DIR/lib/libfbclient.so" ]; then
    echo "ERROR: libfbclient.so missing after install" >&2
    ls -la "$OUTPUT_DIR/lib/" >&2
    exit 1
fi

if [ ! -f "$OUTPUT_DIR/include/ibase.h" ]; then
    echo "ERROR: ibase.h missing after install" >&2
    ls -la "$OUTPUT_DIR/include/" >&2
    exit 1
fi

# -----------------------------------------------------------------------------
# Report
# -----------------------------------------------------------------------------

LIB_COUNT=$(find "$OUTPUT_DIR/lib" -name '*.so*' | wc -l)
HEADER_COUNT=$(find "$OUTPUT_DIR/include" -type f | wc -l)

cat << EOF

Firebird client SDK installed:
  Version:    ${FB_VERSION}
  Arch:       ${ARCH}
  Output:     ${OUTPUT_DIR}
  Libs:       ${LIB_COUNT} files in ${OUTPUT_DIR}/lib/
  Headers:    ${HEADER_COUNT} files in ${OUTPUT_DIR}/include/
  Cache:      ${TARBALL_PATH}

Verify with:
  ls -la ${OUTPUT_DIR}/lib/libfbclient.so*
  test -f ${OUTPUT_DIR}/include/ibase.h && echo OK

EOF
